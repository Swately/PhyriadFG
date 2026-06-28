// framework/hal/include/phyriad/hal/MemoryOrder.hpp
// Barreras de memoria semánticas, portables a weak-memory architectures.
//
// REGLA GLOBAL (enforced por scripts/lint_hal.sh):
//   El código fuera de framework/hal/ NUNCA usa memory_order_* directamente.
//   Toda operación atómica con semántica de sincronización pasa por
//   las funciones de este header.
//
// Por qué importa en ARM:
//   En x86 (TSO), memory_order_release en un store y memory_order_acquire en
//   un load se compilan a instrucciones normales con sólo un compiler fence.
//   En ARM (weak order), memory_order_release compila a STLR y
//   memory_order_acquire compila a LDAR. Sin ellos, el reader ARM puede
//   ver el seq nuevo ANTES que el payload — torn read garantizado.
//
// Categorías:
//   1. seq_store_release / seq_load_acquire — hot path del ring (CRÍTICO)
//   2. ctrl_store_release / ctrl_load_acquire — mensajes de control
//   3. stat_store_relaxed / stat_load_relaxed / stat_fetch_add_relaxed — contadores
//   4. full_fence — barrera completa (paths fríos: shutdown, handshake)
//   5. spin_hint — sugerencia de spin loop (PAUSE/YIELD)
//   6. compiler_fence — sólo barrera de compilador
//
// Uso:
//   hal::seq_store_release(write_idx_, new_val);  // producer publica
//   const auto w = hal::seq_load_acquire(write_idx_);  // consumer lee
//

#pragma once
#include "Arch.hpp"
#include <atomic>
#include <cstddef>

namespace phyriad::hal {

// ─────────────────────────────────────────────────────────────────────────────
// 1. HOT PATH: publicación / consumo del ring.
// ─────────────────────────────────────────────────────────────────────────────

// Publicar índice / seq: garantiza que TODOS los writes previos al payload
// son visibles al consumer antes de que éste lea el nuevo índice.
// x86: store + compiler fence (TSO garantiza el orden hardware).
// ARM: STLR (Store-Release).
template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE void seq_store_release(std::atomic<T>& a, U v) noexcept {
    a.store(static_cast<T>(v), std::memory_order_release);
}

// Leer índice / seq: garantiza que TODOS los reads posteriores al payload
// ven los datos escritos por el producer antes de su seq_store_release.
// x86: load + compiler fence.
// ARM: LDAR (Load-Acquire).
template <class T>
[[nodiscard]] PHYRIAD_ALWAYS_INLINE T seq_load_acquire(const std::atomic<T>& a) noexcept {
    return a.load(std::memory_order_acquire);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. CONTROL PLANE: estado de workers, handshake, cursors.
// ─────────────────────────────────────────────────────────────────────────────
// Semánticamente idénticos a seq_* pero con nombre distinto para claridad.
// El lector del código sabe inmediatamente qué está sincronizando.

template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE void ctrl_store_release(std::atomic<T>& a, U v) noexcept {
    a.store(static_cast<T>(v), std::memory_order_release);
}

template <class T>
[[nodiscard]] PHYRIAD_ALWAYS_INLINE T ctrl_load_acquire(const std::atomic<T>& a) noexcept {
    return a.load(std::memory_order_acquire);
}

// fetch_add con semántica acquire+release (RMW del plano de control).
// Para un contador-cursor compartido donde el incremento PUBLICA escritas
// previas del thread (release) Y observa las escritas de los demás antes de
// decidir (acquire). El caso de uso canónico es el contador "fragments_done"
// del pilar holons: cada worker escribe su partial_results[f] (stores planos)
// y luego suma su avance con este RMW; el último en salir (el que observa
// new_total == total) lee todos los partials con garantía de happens-before.
// Ver HOLON_IMPLEMENTATION_STRATEGIES.md §5.3.
// x86: LOCK XADD (ya es una barrera completa). ARM: LDAXR/STLXR con acq+rel.
template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE T ctrl_fetch_add_acqrel(std::atomic<T>& a, U delta) noexcept {
    return a.fetch_add(static_cast<T>(delta), std::memory_order_acq_rel);
}

// fetch_sub con semántica acquire+release (RMW del plano de control). Misma
// razón que ctrl_fetch_add_acqrel pero para una cuenta DESCENDENTE: el holon
// usa esto en su contador "tasks_active" — la última claim-task en salir (la
// que lleva el contador a 0) resuelve al waiter padre y libera la unidad, con
// garantía de que ya observa el reduce de la última que completó el trabajo.
// Ver HOLON_IMPLEMENTATION_STRATEGIES.md §8 / lifetime.
template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE T ctrl_fetch_sub_acqrel(std::atomic<T>& a, U delta) noexcept {
    return a.fetch_sub(static_cast<T>(delta), std::memory_order_acq_rel);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. ESTADÍSTICAS: contadores no-sincronizantes.
// ─────────────────────────────────────────────────────────────────────────────
// Relaxed: el compilador puede reordenar libremente con otras operaciones.
// Uso correcto: contadores que se suman / leen sin depender de otros datos.

template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE void stat_store_relaxed(std::atomic<T>& a, U v) noexcept {
    a.store(static_cast<T>(v), std::memory_order_relaxed);
}

template <class T>
[[nodiscard]] PHYRIAD_ALWAYS_INLINE T stat_load_relaxed(const std::atomic<T>& a) noexcept {
    return a.load(std::memory_order_relaxed);
}

// fetch_add relaxed para contadores estadísticos.
template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE T stat_fetch_add_relaxed(std::atomic<T>& a, U delta) noexcept {
    return a.fetch_add(static_cast<T>(delta), std::memory_order_relaxed);
}

// fetch_sub relaxed para contadores estadísticos (p.ej. in-flight = dispatched −
// completed: el clasificador suma al despachar, el agregador resta al completar;
// ninguno depende del otro para sincronizar datos → relaxed es correcto).
template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE T stat_fetch_sub_relaxed(std::atomic<T>& a, U delta) noexcept {
    return a.fetch_sub(static_cast<T>(delta), std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. FULL FENCE: barrera completa (paths fríos).
// ─────────────────────────────────────────────────────────────────────────────
// Barrera seq_cst completa. x86: GCC emite `lock or $0,(%rsp)` (un no-op con prefijo
// lock = barrera completa, verificado por scripts/verify_hal_codegen.sh; NO MFENCE en
// GCC), MSVC mfence/__faststorefence. ARM: DMB ISH.
// Coste: ~20-100 ciclos. NO usar en hot path.
PHYRIAD_ALWAYS_INLINE void full_fence() noexcept {
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. SPIN HINT: señal al CPU de que estamos en spin-wait.
// ─────────────────────────────────────────────────────────────────────────────
// x86: PAUSE — reduce contención en HyperThreading, ahorra energía.
// ARM: YIELD — señal de spin al pipeline.
PHYRIAD_ALWAYS_INLINE void spin_hint() noexcept {
#if PHYRIAD_ARCH_X86_64
#  if PHYRIAD_COMPILER_MSVC
    _mm_pause();
#  else
    __builtin_ia32_pause();
#  endif
#elif PHYRIAD_ARCH_AARCH64
    __asm__ volatile("yield" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. COMPILER FENCE: barrera puramente de compilador.
// ─────────────────────────────────────────────────────────────────────────────
// Evita que el compilador reordene reads/writes alrededor de este punto.
// El hardware puede seguir reordenando en ARM.
PHYRIAD_ALWAYS_INLINE void compiler_fence() noexcept {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. SEQLOCK BARRERAS DIRECCIONALES + STORE RELAJADO (single-writer seqlock).
// ─────────────────────────────────────────────────────────────────────────────
// Un seqlock de un solo escritor (Latest<T>) necesita barreras DIRECCIONALES que
// un load-acquire / store-release de una vía no dan:
//   - lector: una valla LoadLoad ENTRE la lectura del payload y la re-verificación
//     del seq (un acquire-load posterior NO impide que la lectura previa se hunda
//     por debajo de él — [atomics.order]p2).
//   - escritor: una valla StoreStore ENTRE el odd-bump y la escritura del payload
//     (un release-store en el odd-bump NO ancla la escritura posterior por encima).
// En x86-TSO ambas compilan a una barrera de compilador (LoadLoad/StoreStore ya
// están prohibidos por el hardware); en ARM emiten el DMB necesario. El odd-bump
// relajado reemplaza un `lock xadd` serializante — el único escritor es el dueño
// de la secuencia, así que el bump es un store plano, no un RMW. Forma canónica
// Rigtorp/Boehm; ver CPU_SUBSTRATE_PRIOR_ART.md Apéndice A.
PHYRIAD_ALWAYS_INLINE void seq_fence_acquire() noexcept {
    std::atomic_thread_fence(std::memory_order_acquire);
}
PHYRIAD_ALWAYS_INLINE void seq_fence_release() noexcept {
    std::atomic_thread_fence(std::memory_order_release);
}
template <class T, class U = T>
PHYRIAD_ALWAYS_INLINE void seq_store_relaxed(std::atomic<T>& a, U v) noexcept {
    a.store(static_cast<T>(v), std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// NOTA SOBRE CAS y fetch_add:
// ─────────────────────────────────────────────────────────────────────────────
// compare_exchange y fetch_add con semántica específica (acq_rel, etc.) NO
// tienen wrapper porque la semántica depende del contexto del caller.
// El linter los permite cuando el site tiene el comentario:
//   // HAL: acq_rel CAS — <razón>

} // namespace phyriad::hal
// Made with my soul - Swately <3
