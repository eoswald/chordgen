#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* =========================================================
 * Anchor runtime — raw 64-bit value representation
 * =========================================================
 *
 * AnchorVal is a single 64-bit word.
 * Integers: raw signed 64-bit value.
 * Pointers: raw address (from malloc-backed arenas).
 * null = 0.
 *
 * All arithmetic operates directly on raw values — zero overhead vs C.
 * ========================================================= */

typedef uint64_t AnchorVal;

#define ANCHOR_NIL ((AnchorVal)0)

/* Pointer → void*: direct cast */
#define _ANCH_HPTR(v)  ((void*)(uintptr_t)(v))

/* Integer value: identity cast */
#define _ANCH_IVAL(v)  ((intptr_t)(int64_t)(v))

/* Float64: reinterpret bits as double */
#define _ANCH_FVAL(v) ({ AnchorVal _av = (v); double _fv; __builtin_memcpy(&_fv, &_av, sizeof(double)); _fv; })

/* Float32: reinterpret low 32 bits as float */
#define _ANCH_F32VAL(v) ({ uint32_t _bv = (uint32_t)(v); float _fv; __builtin_memcpy(&_fv, &_bv, sizeof(float)); _fv; })

#if defined(__GNUC__) || defined(__clang__)
#  define ANCHOR_PURE __attribute__((const))
#else
#  define ANCHOR_PURE
#endif

/* ---- Arena ---- */

typedef struct _AnchorArena {
    char*                buf;
    size_t               cap;
    size_t               used;
    size_t               checkpoint;
    struct _AnchorArena* prev;
} _AnchorArena;

#ifdef ANCHOR_MULTI_THREADED
static _Thread_local _AnchorArena* _anchor_arena_top = NULL;
#else
static _AnchorArena* _anchor_arena_top = NULL;
#endif
#define ANCHOR_DEFAULT_ARENA_CAP (1024 * 1024)

static inline AnchorVal anchor_alloc(size_t size) {
    _AnchorArena* a = _anchor_arena_top;
    if (!a) __builtin_trap();
    size_t aligned = (size + 7u) & ~7u;
    if (a->used + aligned > a->cap) __builtin_trap();
    AnchorVal r = (AnchorVal)(uintptr_t)(a->buf + a->used);
    a->used += aligned;
    return r;
}

static inline void _anchor_arena_reset(_AnchorArena* a) { a->used = 0; }

/* Wrap a raw C pointer as an AnchorVal */
static inline ANCHOR_PURE AnchorVal anchor_ext(void* p) {
    return p ? (AnchorVal)(uintptr_t)p : ANCHOR_NIL;
}

/* Unwrap AnchorVal pointer → void* */
static inline void* _anch_ptr(AnchorVal v) { return (void*)(uintptr_t)v; }

/* ---- Integer constructors / arithmetic — all raw, zero overhead vs C ---- */

static inline ANCHOR_PURE AnchorVal anchor_int(intptr_t v)  { return (AnchorVal)(int64_t)v; }
static inline ANCHOR_PURE AnchorVal anchor_add(AnchorVal a, AnchorVal b) { return a + b; }
static inline ANCHOR_PURE AnchorVal anchor_sub(AnchorVal a, AnchorVal b) { return a - b; }
static inline ANCHOR_PURE AnchorVal anchor_mul(AnchorVal a, AnchorVal b) { return a * b; }
static inline ANCHOR_PURE AnchorVal anchor_div(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a / (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_mod(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a % (int64_t)b); }

/* ---- Float constructor / arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_float(double v) {
    AnchorVal bits; __builtin_memcpy(&bits, &v, sizeof(double)); return bits;
}
static inline ANCHOR_PURE AnchorVal anchor_addf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) + _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_subf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) - _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_mulf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) * _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_divf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) / _ANCH_FVAL(b)); }

/* ---- Float32 constructor / arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_f32(float v) {
    uint32_t bits; __builtin_memcpy(&bits, &v, sizeof(float)); return (AnchorVal)bits;
}
static inline ANCHOR_PURE AnchorVal anchor_addf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) + _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_subf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) - _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_mulf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) * _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_divf32(AnchorVal a, AnchorVal b) { return anchor_f32(_ANCH_F32VAL(a) / _ANCH_F32VAL(b)); }

/* ---- Comparisons — direct on raw values ---- */

static inline ANCHOR_PURE AnchorVal anchor_eq(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a == (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_ne(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a != (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_lt(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a <  (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_gt(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a >  (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_le(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a <= (int64_t)b); }
static inline ANCHOR_PURE AnchorVal anchor_ge(AnchorVal a, AnchorVal b) { return (AnchorVal)((int64_t)a >= (int64_t)b); }

/* ---- Bitwise ---- */

static inline ANCHOR_PURE AnchorVal anchor_band(AnchorVal a, AnchorVal b)   { return a & b; }
static inline ANCHOR_PURE AnchorVal anchor_bor (AnchorVal a, AnchorVal b)   { return a | b; }
static inline ANCHOR_PURE AnchorVal anchor_bxor(AnchorVal a, AnchorVal b)   { return a ^ b; }
static inline ANCHOR_PURE AnchorVal anchor_bnot(AnchorVal a)                { return ~a; }
static inline ANCHOR_PURE AnchorVal anchor_lshift(AnchorVal a, AnchorVal b) { return a << b; }
static inline ANCHOR_PURE AnchorVal anchor_rshift(AnchorVal a, AnchorVal b) { return (AnchorVal)((uint64_t)a >> b); }

/* ---- Unsigned arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_addu(AnchorVal a, AnchorVal b) { return a + b; }
static inline ANCHOR_PURE AnchorVal anchor_subu(AnchorVal a, AnchorVal b) { return a - b; }
static inline ANCHOR_PURE AnchorVal anchor_mulu(AnchorVal a, AnchorVal b) { return a * b; }
static inline ANCHOR_PURE AnchorVal anchor_divu(AnchorVal a, AnchorVal b) { return a / b; }
static inline ANCHOR_PURE AnchorVal anchor_modu(AnchorVal a, AnchorVal b) { return a % b; }
static inline ANCHOR_PURE AnchorVal anchor_ltu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a <  b); }
static inline ANCHOR_PURE AnchorVal anchor_gtu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a >  b); }
static inline ANCHOR_PURE AnchorVal anchor_leu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a <= b); }
static inline ANCHOR_PURE AnchorVal anchor_geu (AnchorVal a, AnchorVal b) { return (AnchorVal)(a >= b); }

/* ---- Float comparisons ---- */

static inline ANCHOR_PURE AnchorVal anchor_eqf(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) == _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_nef(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) != _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_ltf(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) <  _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gtf(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) >  _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_lef(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) <= _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gef(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_FVAL(a) >= _ANCH_FVAL(b)); }

/* ---- Float32 comparisons ---- */

static inline ANCHOR_PURE AnchorVal anchor_eqf32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) == _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_nef32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) != _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_ltf32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) <  _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gtf32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) >  _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_lef32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) <= _ANCH_F32VAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_gef32(AnchorVal a, AnchorVal b) { return (AnchorVal)(_ANCH_F32VAL(a) >= _ANCH_F32VAL(b)); }

/* ---- Logical ---- */

static inline ANCHOR_PURE AnchorVal anchor_not(AnchorVal a)              { return (AnchorVal)(!a); }


/* struct Cons */
#define ANCHOR_SIZEOF_Cons 16
#define ANCHOR_OFFSET_Cons_car 0
#define ANCHOR_SIZE_Cons_car  8
#define ANCHOR_OFFSET_Cons_cdr 8
#define ANCHOR_SIZE_Cons_cdr  8

#include <portmidi.h>
#include <porttime.h>
#include <stdio.h>
#include <time.h>
AnchorVal string_gt_symbol(AnchorVal s);
AnchorVal gen_chord(AnchorVal chords, AnchorVal num_chords, AnchorVal inversions, AnchorVal num_inversions, AnchorVal result);
AnchorVal find_midi_input_device(void);
AnchorVal read_chord(AnchorVal midi, AnchorVal buf);


AnchorVal string_gt_symbol(AnchorVal s) {
    int _anc_t0_raw = strlen(((const char*)_anch_ptr(s)));
    AnchorVal _anc_t0 = anchor_int((intptr_t)_anc_t0_raw);
    AnchorVal slen = _anc_t0;
    AnchorVal r = anchor_int(0);
    if (_ANCH_IVAL(anchor_lt(slen, anchor_int(8)))) {
        memcpy(((void*)_anch_ptr(anchor_ext((void*)&r))), ((void*)_anch_ptr(s)), (size_t)_ANCH_IVAL(slen));
        return r;
    } else {
        return r;
    }
    return anchor_int(0);
}

AnchorVal gen_chord(AnchorVal chords, AnchorVal num_chords, AnchorVal inversions, AnchorVal num_inversions, AnchorVal result) {
    int _anc_t1_raw = rand();
    AnchorVal _anc_t1 = anchor_int((intptr_t)_anc_t1_raw);
    AnchorVal chord_idx = anchor_mod(_anc_t1, num_chords);
    AnchorVal _anc_t2 = 0;
    __builtin_memcpy(&_anc_t2, (char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_mul(chord_idx, anchor_int(8))), 8);
    AnchorVal chord = _anc_t2;
    int _anc_t3_raw = rand();
    AnchorVal _anc_t3 = anchor_int((intptr_t)_anc_t3_raw);
    AnchorVal inversion_idx = anchor_mod(_anc_t3, num_inversions);
    AnchorVal _anc_t4 = 0;
    __builtin_memcpy(&_anc_t4, (char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_mul(inversion_idx, anchor_int(16))), 8);
    AnchorVal inversion = _anc_t4;
    sprintf(((char*)_anch_ptr(result)), "%s %s", ((char*)_anch_ptr(chord)), ((char*)_anch_ptr(inversion)));
    return result;
}

AnchorVal find_midi_input_device(void) {
    char _anc_arena_buf[ANCHOR_DEFAULT_ARENA_CAP];
    _AnchorArena _anc_arena = {_anc_arena_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
    _anchor_arena_top = &_anc_arena;
    {
        AnchorVal i = anchor_int(0);
        int _anc_t5_raw;
        AnchorVal _anc_t5;
        _anc_t5_raw = Pm_CountDevices();
        _anc_t5 = anchor_int((intptr_t)_anc_t5_raw);
        while (_ANCH_IVAL(anchor_lt(i, _anc_t5))) {
            const void* _anc_t6_raw = Pm_GetDeviceInfo((int)_ANCH_IVAL(i));
            AnchorVal _anc_t6 = anchor_ext((void*)_anc_t6_raw);
            AnchorVal info = _anc_t6;
            AnchorVal _anc_t7 = 0;
            __builtin_memcpy(&_anc_t7, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(16)), 8);
            AnchorVal name = anchor_ext((char*)_anch_ptr(_anc_t7));
            AnchorVal _anc_t8 = 0;
            __builtin_memcpy(&_anc_t8, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(28)), 4);
            AnchorVal is_output = _anc_t8;
            printf("Device %d: %s is-output=%d\n", (int)_ANCH_IVAL(i), ((char*)_anch_ptr(name)), (int)_ANCH_IVAL(is_output));
            if (_ANCH_IVAL((AnchorVal)(!!info && !!anchor_eq(is_output, anchor_int(0))))) {
                _anchor_arena_top = _anc_arena.prev;
                return i;
            }
            i = anchor_add(i, anchor_int(1));
            _anc_t5_raw = Pm_CountDevices();
            _anc_t5 = anchor_int((intptr_t)_anc_t5_raw);
        }
    }
    _anchor_arena_top = _anc_arena.prev;
    return anchor_int(-1);
}

AnchorVal read_chord(AnchorVal midi, AnchorVal buf) {
    char _anc_arena_buf[ANCHOR_DEFAULT_ARENA_CAP];
    _AnchorArena _anc_arena = {_anc_arena_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
    _anchor_arena_top = &_anc_arena;
    while (_ANCH_IVAL(anchor_int(1))) {
        AnchorVal _anc_t9 = 0;
        __builtin_memcpy(&_anc_t9, (char*)_ANCH_HPTR(midi) + _ANCH_IVAL(anchor_int(0)), 8);
        int _anc_t10_raw = Pm_Poll(((void*)_anch_ptr(_anc_t9)));
        AnchorVal _anc_t10 = anchor_int((intptr_t)_anc_t10_raw);
        if (_ANCH_IVAL(anchor_eq(_anc_t10, anchor_int(1)))) {
            AnchorVal _anc_t11 = 0;
            __builtin_memcpy(&_anc_t11, (char*)_ANCH_HPTR(midi) + _ANCH_IVAL(anchor_int(0)), 8);
            int _anc_t12_raw = Pm_Read(((void*)_anch_ptr(_anc_t11)), ((void*)_anch_ptr(buf)), 32);
            AnchorVal _anc_t12 = anchor_int((intptr_t)_anc_t12_raw);
            AnchorVal count = _anc_t12;
            {
                AnchorVal i = anchor_int(0);
                while (_ANCH_IVAL(anchor_lt(i, count))) {
                    AnchorVal _anc_t13 = 0;
                    __builtin_memcpy(&_anc_t13, (char*)_ANCH_HPTR(buf) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 4);
                    AnchorVal message = _anc_t13;
                    AnchorVal _anc_t14 = 0;
                    __builtin_memcpy(&_anc_t14, (char*)_ANCH_HPTR(buf) + _ANCH_IVAL(anchor_add(anchor_mul(i, anchor_int(8)), anchor_int(4))), 4);
                    AnchorVal timestamp = _anc_t14;
                    AnchorVal status = anchor_band(message, anchor_int(255));
                    AnchorVal note = anchor_band(anchor_rshift(message, anchor_int(8)), anchor_int(255));
                    AnchorVal velocity = anchor_band(anchor_rshift(message, anchor_int(16)), anchor_int(255));
                    printf("Received MIDI event: status=%d, note=%d, velocity=%d at timestamp: %d\n", (int)_ANCH_IVAL(status), (int)_ANCH_IVAL(note), (int)_ANCH_IVAL(velocity), (int)_ANCH_IVAL(timestamp));
                    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(status, anchor_int(144)) && !!anchor_eq(note, anchor_int(60))))) {
                        printf("Middle C pressed!\n");
                        _anchor_arena_top = _anc_arena.prev;
                        return anchor_int(-1);
                    }
                    i = anchor_add(i, anchor_int(1));
                }
            }
        }
        Pt_Sleep(10);
    }
    _anchor_arena_top = _anc_arena.prev;
    return anchor_int(0);
}

int main(void) {
    char _anc_arena_buf[ANCHOR_DEFAULT_ARENA_CAP];
    _AnchorArena _anc_arena = {_anc_arena_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
    _anchor_arena_top = &_anc_arena;
    AnchorVal num_chords = anchor_int(18);
    AnchorVal chords = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(num_chords, anchor_int(8))));
    { AnchorVal _anc_t15 = anchor_ext((void*)"Cmaj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(0)), &_anc_t15, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t16 = anchor_ext((void*)"Cm");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(8)), &_anc_t16, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t17 = anchor_ext((void*)"Dmaj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(16)), &_anc_t17, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t18 = anchor_ext((void*)"Dm");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(24)), &_anc_t18, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t19 = anchor_ext((void*)"E♭maj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(32)), &_anc_t19, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t20 = anchor_ext((void*)"Em");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(40)), &_anc_t20, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t21 = anchor_ext((void*)"Edim");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(48)), &_anc_t21, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t22 = anchor_ext((void*)"Fmaj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(56)), &_anc_t22, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t23 = anchor_ext((void*)"F♯maj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(64)), &_anc_t23, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t24 = anchor_ext((void*)"Gmaj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(72)), &_anc_t24, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t25 = anchor_ext((void*)"Gm");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(80)), &_anc_t25, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t26 = anchor_ext((void*)"Amaj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(88)), &_anc_t26, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t27 = anchor_ext((void*)"Am");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(96)), &_anc_t27, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t28 = anchor_ext((void*)"Adim");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(104)), &_anc_t28, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t29 = anchor_ext((void*)"B♭maj");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(112)), &_anc_t29, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t30 = anchor_ext((void*)"Bm");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(120)), &_anc_t30, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t31 = anchor_ext((void*)"Bdim");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(128)), &_anc_t31, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t32 = anchor_ext((void*)"C♯m");
      __builtin_memcpy((char*)_ANCH_HPTR(chords) + _ANCH_IVAL(anchor_int(136)), &_anc_t32, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    AnchorVal num_inversions = anchor_int(3);
    AnchorVal inversions = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(3), anchor_int(16))));
    { AnchorVal _anc_t33 = anchor_ext((void*)"root");
      __builtin_memcpy((char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_int(0)), &_anc_t33, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t34 = anchor_ext((void*)"1st inversion");
      __builtin_memcpy((char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_int(16)), &_anc_t34, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t35 = anchor_ext((void*)"2nd inversion");
      __builtin_memcpy((char*)_ANCH_HPTR(inversions) + _ANCH_IVAL(anchor_int(32)), &_anc_t35, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    AnchorVal input_buffer_size = anchor_int(128);
    AnchorVal midi = anchor_alloc(8);
    AnchorVal event = anchor_alloc(8);
    long _anc_t36_raw = time((void*)0);
    AnchorVal _anc_t36 = anchor_int((intptr_t)_anc_t36_raw);
    srand((unsigned int)_ANCH_IVAL(_anc_t36));
    int _anc_t37_raw = Pm_Initialize();
    AnchorVal _anc_t37 = anchor_int((intptr_t)_anc_t37_raw);
    if (_ANCH_IVAL(anchor_ne(_anc_t37, anchor_int(0)))) {
        printf("Failed to initialize PortMidi\n");
        _anchor_arena_top = _anc_arena.prev;
        return 1;
    }
    AnchorVal device_id = find_midi_input_device();
    if (_ANCH_IVAL(anchor_eq(device_id, anchor_int(-1)))) {
        printf("No MIDI input device found\n");
        _anchor_arena_top = _anc_arena.prev;
        return 1;
    }
    int _anc_t38_raw = Pm_OpenInput(((void*)_anch_ptr(midi)), (int)_ANCH_IVAL(device_id), 0, (int)_ANCH_IVAL(input_buffer_size), (void*)0, 0);
    AnchorVal _anc_t38 = anchor_int((intptr_t)_anc_t38_raw);
    if (_ANCH_IVAL(anchor_ne(_anc_t38, anchor_int(0)))) {
        printf("Failed to open MIDI input\n");
        _anchor_arena_top = _anc_arena.prev;
        return 1;
    }
    while (_ANCH_IVAL(anchor_int(1))) {
        printf("Press Enter to generate a random chord...\n");
        fgets(((char*)_anch_ptr(anchor_alloc(8))), 8, ((void*)_anch_ptr(anchor_int((intptr_t)(stdin)))));
        AnchorVal result = anchor_alloc(32);
        printf("Play this chord: %s\n\n", ((char*)_anch_ptr(gen_chord(chords, num_chords, inversions, num_inversions, result))));
        read_chord(midi, event);
    }
    _anchor_arena_top = _anc_arena.prev;
    return 0;
}