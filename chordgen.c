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
static inline double _ANCH_FVAL(AnchorVal v) {
    double _fv; memcpy(&_fv, &v, sizeof(double)); return _fv;
}

/* Float32: reinterpret low 32 bits as float */
static inline float _ANCH_F32VAL(AnchorVal v) {
    uint32_t _bv = (uint32_t)v; float _fv; memcpy(&_fv, &_bv, sizeof(float)); return _fv;
}

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
#define ANCHOR_DEFAULT_ARENA_CAP 65536

static inline AnchorVal anchor_alloc(size_t size) {
    _AnchorArena* a = _anchor_arena_top;
    if (!a) abort();
    size_t aligned = (size + 7u) & ~7u;
    if (a->used + aligned > a->cap) abort();
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
    AnchorVal bits; memcpy(&bits, &v, sizeof(double)); return bits;
}
static inline ANCHOR_PURE AnchorVal anchor_addf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) + _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_subf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) - _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_mulf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) * _ANCH_FVAL(b)); }
static inline ANCHOR_PURE AnchorVal anchor_divf(AnchorVal a, AnchorVal b) { return anchor_float(_ANCH_FVAL(a) / _ANCH_FVAL(b)); }

/* ---- Float32 constructor / arithmetic ---- */

static inline ANCHOR_PURE AnchorVal anchor_f32(float v) {
    uint32_t bits; memcpy(&bits, &v, sizeof(float)); return (AnchorVal)bits;
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

#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <portmidi.h>
#include <porttime.h>
/* struct Exercise */
#define ANCHOR_SIZEOF_Exercise 40
#define ANCHOR_OFFSET_Exercise_keys 0
#define ANCHOR_SIZE_Exercise_keys  8
#define ANCHOR_OFFSET_Exercise_num_keys 8
#define ANCHOR_SIZE_Exercise_num_keys  8
#define ANCHOR_OFFSET_Exercise_progression 16
#define ANCHOR_SIZE_Exercise_progression  8
#define ANCHOR_OFFSET_Exercise_prog_len 24
#define ANCHOR_SIZE_Exercise_prog_len  8
#define ANCHOR_OFFSET_Exercise_reps 32
#define ANCHOR_SIZE_Exercise_reps  8

/* struct Lesson */
#define ANCHOR_SIZEOF_Lesson 24
#define ANCHOR_OFFSET_Lesson_name 0
#define ANCHOR_SIZE_Lesson_name  8
#define ANCHOR_OFFSET_Lesson_exercises 8
#define ANCHOR_SIZE_Lesson_exercises  8
#define ANCHOR_OFFSET_Lesson_num_exercises 16
#define ANCHOR_SIZE_Lesson_num_exercises  8

/* struct Args */
#define ANCHOR_SIZEOF_Args 40
#define ANCHOR_OFFSET_Args_debug 0
#define ANCHOR_SIZE_Args_debug  8
#define ANCHOR_OFFSET_Args_select_device 8
#define ANCHOR_SIZE_Args_select_device  8
#define ANCHOR_OFFSET_Args_lesson_num 16
#define ANCHOR_SIZE_Args_lesson_num  8
#define ANCHOR_OFFSET_Args_list_lessons 24
#define ANCHOR_SIZE_Args_list_lessons  8
#define ANCHOR_OFFSET_Args_help 32
#define ANCHOR_SIZE_Args_help  8

AnchorVal string_gt_symbol(AnchorVal s);
AnchorVal copy_notes(AnchorVal src, AnchorVal dst, AnchorVal count);
AnchorVal note_add(AnchorVal notes, AnchorVal note_count, AnchorVal note);
AnchorVal note_remove(AnchorVal notes, AnchorVal note_count, AnchorVal note);
AnchorVal print_notes(AnchorVal notes, AnchorVal count);
AnchorVal find_midi_input_device(AnchorVal select);
AnchorVal read_chord(AnchorVal midi, AnchorVal buf, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal debug);
AnchorVal key_root(AnchorVal key_id);
AnchorVal key_mode(AnchorVal key_id);
AnchorVal degree_semitones(AnchorVal mode, AnchorVal degree);
AnchorVal degree_chord_type(AnchorVal mode, AnchorVal degree);
AnchorVal degree_roman(AnchorVal degree);
AnchorVal chord_name_for_degree(AnchorVal key_id, AnchorVal degree, AnchorVal inversion, AnchorVal note_names, AnchorVal result);
AnchorVal key_name(AnchorVal key_id, AnchorVal note_names, AnchorVal result);
AnchorVal identify_chord(AnchorVal notes, AnchorVal count, AnchorVal note_names, AnchorVal result);
AnchorVal parse_args(AnchorVal argc, AnchorVal argv);
AnchorVal print_help(void);
AnchorVal print_lesson_list(AnchorVal lessons, AnchorVal num_lessons);
AnchorVal select_lesson_menu(AnchorVal lessons, AnchorVal num_lessons);
AnchorVal print_progression_line(AnchorVal prog, AnchorVal prog_len, AnchorVal note_names, AnchorVal key_id, AnchorVal chord_bufs);
AnchorVal challenge(AnchorVal degree, AnchorVal target, AnchorVal midi, AnchorVal event, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal note_names, AnchorVal debug);
AnchorVal run_exercise(AnchorVal ex, AnchorVal midi, AnchorVal event, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal note_names, AnchorVal debug);
AnchorVal run_lesson(AnchorVal lesson, AnchorVal midi, AnchorVal event, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal note_names, AnchorVal debug);


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

AnchorVal copy_notes(AnchorVal src, AnchorVal dst, AnchorVal count) {
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t1 = *(AnchorVal*)((char*)_ANCH_HPTR(src) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            { AnchorVal _anc_t2 = _anc_t1;
              memcpy((char*)_ANCH_HPTR(dst) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), &_anc_t2, sizeof(AnchorVal)); }
            ANCHOR_NIL;
            i = anchor_add(i, anchor_int(1));
        }
    }
    return anchor_int(0);
}

AnchorVal note_add(AnchorVal notes, AnchorVal note_count, AnchorVal note) {
    AnchorVal _anc_t3 = *(AnchorVal*)((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)));
    AnchorVal count = _anc_t3;
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t4 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            if (_ANCH_IVAL(anchor_eq(_anc_t4, note))) {
                return anchor_int(0);
            }
            i = anchor_add(i, anchor_int(1));
        }
    }
    { AnchorVal _anc_t5 = note;
      memcpy((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(count, anchor_int(8))), &_anc_t5, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t6 = anchor_add(count, anchor_int(1));
      memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t6, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    return anchor_int(0);
}

AnchorVal note_remove(AnchorVal notes, AnchorVal note_count, AnchorVal note) {
    AnchorVal _anc_t7 = *(AnchorVal*)((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)));
    AnchorVal count = _anc_t7;
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t8 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            if (_ANCH_IVAL(anchor_eq(_anc_t8, note))) {
                {
                    AnchorVal j = i;
                    while (_ANCH_IVAL(anchor_lt(j, anchor_sub(count, anchor_int(1))))) {
                        AnchorVal _anc_t9 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(anchor_add(j, anchor_int(1)), anchor_int(8))));
                        { AnchorVal _anc_t10 = _anc_t9;
                          memcpy((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(j, anchor_int(8))), &_anc_t10, sizeof(AnchorVal)); }
                        ANCHOR_NIL;
                        j = anchor_add(j, anchor_int(1));
                    }
                }
                { AnchorVal _anc_t11 = anchor_sub(count, anchor_int(1));
                  memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t11, sizeof(AnchorVal)); }
                ANCHOR_NIL;
                return anchor_int(0);
            }
            i = anchor_add(i, anchor_int(1));
        }
    }
    return anchor_int(0);
}

AnchorVal print_notes(AnchorVal notes, AnchorVal count) {
    printf("Notes (%d):", (int)_ANCH_IVAL(count));
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, count))) {
            AnchorVal _anc_t12 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            printf(" %d", (int)_ANCH_IVAL(_anc_t12));
            i = anchor_add(i, anchor_int(1));
        }
    }
    printf("\n");
    return anchor_int(0);
}

AnchorVal find_midi_input_device(AnchorVal select) {
    if (_ANCH_IVAL(anchor_not(select))) {
        {
            AnchorVal i = anchor_int(0);
            while (1) {
                int _anc_t13_raw = Pm_CountDevices();
                AnchorVal _anc_t13 = anchor_int((intptr_t)_anc_t13_raw);
                if (!_ANCH_IVAL(anchor_lt(i, _anc_t13))) break;
                const void* _anc_t14_raw = Pm_GetDeviceInfo((int)_ANCH_IVAL(i));
                AnchorVal _anc_t14 = anchor_ext((void*)_anc_t14_raw);
                AnchorVal info = _anc_t14;
                AnchorVal _anc_t15 = 0;
                memcpy(&_anc_t15, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(28)), 4);
                AnchorVal is_output = _anc_t15;
                if (_ANCH_IVAL(anchor_eq(is_output, anchor_int(0)))) {
                    return i;
                }
                i = anchor_add(i, anchor_int(1));
            }
        }
        return anchor_int(-1);
    }
    {
        char _anc_arena__anc_t16_buf[ANCHOR_DEFAULT_ARENA_CAP];
        _AnchorArena _anc_arena__anc_t16 = {_anc_arena__anc_t16_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
        _anchor_arena_top = &_anc_arena__anc_t16;
        printf("MIDI input devices:\n");
        {
            AnchorVal i = anchor_int(0);
            while (1) {
                int _anc_t17_raw = Pm_CountDevices();
                AnchorVal _anc_t17 = anchor_int((intptr_t)_anc_t17_raw);
                if (!_ANCH_IVAL(anchor_lt(i, _anc_t17))) break;
                const void* _anc_t18_raw = Pm_GetDeviceInfo((int)_ANCH_IVAL(i));
                AnchorVal _anc_t18 = anchor_ext((void*)_anc_t18_raw);
                AnchorVal info = _anc_t18;
                AnchorVal _anc_t19 = *(AnchorVal*)((char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(16)));
                AnchorVal name = anchor_ext((char*)_anch_ptr(_anc_t19));
                AnchorVal _anc_t20 = 0;
                memcpy(&_anc_t20, (char*)_ANCH_HPTR(info) + _ANCH_IVAL(anchor_int(28)), 4);
                AnchorVal is_output = _anc_t20;
                if (_ANCH_IVAL(anchor_eq(is_output, anchor_int(0)))) {
                    printf("  %d: \x1B[96m%s\x1B[0m\n", (int)_ANCH_IVAL(i), ((char*)_anch_ptr(name)));
                }
                i = anchor_add(i, anchor_int(1));
            }
        }
        printf("Enter device ID: ");
        AnchorVal buf = anchor_alloc(16);
        fgets(((char*)_anch_ptr(buf)), 16, ((void*)_anch_ptr(anchor_int((intptr_t)(stdin)))));
        int _anc_t21_raw = atoi(((char*)_anch_ptr(buf)));
        AnchorVal _anc_t21 = anchor_int((intptr_t)_anc_t21_raw);
        _anchor_arena_top = _anc_arena__anc_t16.prev;
        return _anc_t21;
        _anchor_arena_top = _anc_arena__anc_t16.prev;
    }
    return anchor_int(0);
}

AnchorVal read_chord(AnchorVal midi, AnchorVal buf, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal debug) {
    AnchorVal started = anchor_int(0);
    while (_ANCH_IVAL(anchor_int(1))) {
        AnchorVal _anc_t22 = *(AnchorVal*)((char*)_ANCH_HPTR(midi) + _ANCH_IVAL(anchor_int(0)));
        int _anc_t23_raw = Pm_Poll(((void*)_anch_ptr(_anc_t22)));
        AnchorVal _anc_t23 = anchor_int((intptr_t)_anc_t23_raw);
        if (_ANCH_IVAL(anchor_eq(_anc_t23, anchor_int(1)))) {
            AnchorVal _anc_t24 = *(AnchorVal*)((char*)_ANCH_HPTR(midi) + _ANCH_IVAL(anchor_int(0)));
            int _anc_t25_raw = Pm_Read(((void*)_anch_ptr(_anc_t24)), ((void*)_anch_ptr(buf)), 32);
            AnchorVal _anc_t25 = anchor_int((intptr_t)_anc_t25_raw);
            AnchorVal count = _anc_t25;
            {
                AnchorVal i = anchor_int(0);
                while (_ANCH_IVAL(anchor_lt(i, count))) {
                    AnchorVal _anc_t26 = 0;
                    memcpy(&_anc_t26, (char*)_ANCH_HPTR(buf) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), 4);
                    AnchorVal message = _anc_t26;
                    AnchorVal status = anchor_band(message, anchor_int(255));
                    AnchorVal note = anchor_band(anchor_rshift(message, anchor_int(8)), anchor_int(255));
                    AnchorVal velocity = anchor_band(anchor_rshift(message, anchor_int(16)), anchor_int(255));
                    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(status, anchor_int(144)) && !!anchor_ne(velocity, anchor_int(0))))) {
                        note_add(active_notes, note_count, note);
                        started = anchor_int(1);
                        AnchorVal _anc_t27 = *(AnchorVal*)((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)));
                        AnchorVal nc = _anc_t27;
                        copy_notes(active_notes, last_notes, nc);
                        { AnchorVal _anc_t28 = nc;
                          memcpy((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t28, sizeof(AnchorVal)); }
                        ANCHOR_NIL;
                        if (_ANCH_IVAL(debug)) {
                            print_notes(active_notes, nc);
                        }
                    }
                    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(status, anchor_int(128)) || !!(AnchorVal)(!!anchor_eq(status, anchor_int(144)) && !!anchor_eq(velocity, anchor_int(0)))))) {
                        note_remove(active_notes, note_count, note);
                        if (_ANCH_IVAL(debug)) {
                            AnchorVal _anc_t29 = *(AnchorVal*)((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)));
                            print_notes(active_notes, _anc_t29);
                        }
                        AnchorVal _anc_t31 = (AnchorVal)(!!started);
                        if (_anc_t31) {
                            AnchorVal _anc_t30 = *(AnchorVal*)((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)));
                            _anc_t31 = (AnchorVal)(!!anchor_eq(_anc_t30, anchor_int(0)));
                        }
                        if (_ANCH_IVAL(_anc_t31)) {
                            return anchor_int(0);
                        }
                    }
                    i = anchor_add(i, anchor_int(1));
                }
            }
        }
        Pt_Sleep(10);
    }
    return anchor_int(0);
}

AnchorVal key_root(AnchorVal key_id) {
    return anchor_mod(key_id, anchor_int(16));
}

AnchorVal key_mode(AnchorVal key_id) {
    return anchor_div(key_id, anchor_int(16));
}

AnchorVal degree_semitones(AnchorVal mode, AnchorVal degree) {
    AnchorVal d = anchor_sub(degree, anchor_int(1));
    if (_ANCH_IVAL(anchor_eq(mode, anchor_int(0)))) {
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(0)))) {
            return anchor_int(0);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(1)))) {
            return anchor_int(2);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(2)))) {
            return anchor_int(4);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(3)))) {
            return anchor_int(5);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(4)))) {
            return anchor_int(7);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(5)))) {
            return anchor_int(9);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(6)))) {
            return anchor_int(11);
        }
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(0)))) {
        return anchor_int(0);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(1)))) {
        return anchor_int(2);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(2)))) {
        return anchor_int(3);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(3)))) {
        return anchor_int(5);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(4)))) {
        return anchor_int(7);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(5)))) {
        return anchor_int(8);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(6)))) {
        return anchor_int(10);
    }
    return anchor_int(0);
}

AnchorVal degree_chord_type(AnchorVal mode, AnchorVal degree) {
    AnchorVal d = anchor_sub(degree, anchor_int(1));
    if (_ANCH_IVAL(anchor_eq(mode, anchor_int(0)))) {
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(0)))) {
            return anchor_int(0);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(1)))) {
            return anchor_int(1);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(2)))) {
            return anchor_int(1);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(3)))) {
            return anchor_int(0);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(4)))) {
            return anchor_int(0);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(5)))) {
            return anchor_int(1);
        }
        if (_ANCH_IVAL(anchor_eq(d, anchor_int(6)))) {
            return anchor_int(2);
        }
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(0)))) {
        return anchor_int(1);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(1)))) {
        return anchor_int(2);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(2)))) {
        return anchor_int(0);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(3)))) {
        return anchor_int(1);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(4)))) {
        return anchor_int(1);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(5)))) {
        return anchor_int(0);
    }
    if (_ANCH_IVAL(anchor_eq(d, anchor_int(6)))) {
        return anchor_int(0);
    }
    return anchor_int(0);
}

AnchorVal degree_roman(AnchorVal degree) {
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(1)))) {
        return anchor_ext((void*)"I");
    }
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(2)))) {
        return anchor_ext((void*)"II");
    }
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(3)))) {
        return anchor_ext((void*)"III");
    }
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(4)))) {
        return anchor_ext((void*)"IV");
    }
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(5)))) {
        return anchor_ext((void*)"V");
    }
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(6)))) {
        return anchor_ext((void*)"VI");
    }
    if (_ANCH_IVAL(anchor_eq(degree, anchor_int(7)))) {
        return anchor_ext((void*)"VII");
    }
    return anchor_ext((void*)"?");
}

AnchorVal chord_name_for_degree(AnchorVal key_id, AnchorVal degree, AnchorVal inversion, AnchorVal note_names, AnchorVal result) {
    AnchorVal root_pc = key_root(key_id);
    AnchorVal mode = key_mode(key_id);
    AnchorVal offset = degree_semitones(mode, degree);
    AnchorVal chord_root_pc = anchor_mod(anchor_add(root_pc, offset), anchor_int(12));
    AnchorVal ctype = degree_chord_type(mode, degree);
    AnchorVal _anc_t32;
    if (_ANCH_IVAL(anchor_eq(ctype, anchor_int(0)))) {
        _anc_t32 = anchor_ext((void*)"maj");
    } else {
        AnchorVal _anc_t33;
        if (_ANCH_IVAL(anchor_eq(ctype, anchor_int(1)))) {
            _anc_t33 = anchor_ext((void*)"m");
        } else {
            _anc_t33 = anchor_ext((void*)"dim");
        }
        _anc_t32 = _anc_t33;
    }
    AnchorVal type_str = _anc_t32;
    AnchorVal _anc_t34 = *(AnchorVal*)((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_mul(chord_root_pc, anchor_int(8))));
    AnchorVal root_name = _anc_t34;
    AnchorVal _anc_t35;
    if (_ANCH_IVAL(anchor_eq(inversion, anchor_int(0)))) {
        _anc_t35 = anchor_ext((void*)"root");
    } else {
        AnchorVal _anc_t36;
        if (_ANCH_IVAL(anchor_eq(inversion, anchor_int(1)))) {
            _anc_t36 = anchor_ext((void*)"1st inversion");
        } else {
            _anc_t36 = anchor_ext((void*)"2nd inversion");
        }
        _anc_t35 = _anc_t36;
    }
    AnchorVal inv_str = _anc_t35;
    sprintf(((char*)_anch_ptr(result)), "%s%s %s", ((char*)_anch_ptr(root_name)), ((char*)_anch_ptr(type_str)), ((char*)_anch_ptr(inv_str)));
    return result;
}

AnchorVal key_name(AnchorVal key_id, AnchorVal note_names, AnchorVal result) {
    AnchorVal root_pc = key_root(key_id);
    AnchorVal mode = key_mode(key_id);
    AnchorVal _anc_t37 = *(AnchorVal*)((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_mul(root_pc, anchor_int(8))));
    AnchorVal root_name = _anc_t37;
    AnchorVal _anc_t38;
    if (_ANCH_IVAL(anchor_eq(mode, anchor_int(0)))) {
        _anc_t38 = anchor_ext((void*)"major");
    } else {
        _anc_t38 = anchor_ext((void*)"minor");
    }
    AnchorVal mode_str = _anc_t38;
    sprintf(((char*)_anch_ptr(result)), "%s %s", ((char*)_anch_ptr(root_name)), ((char*)_anch_ptr(mode_str)));
    return result;
}

AnchorVal identify_chord(AnchorVal notes, AnchorVal count, AnchorVal note_names, AnchorVal result) {
    if (_ANCH_IVAL(anchor_ne(count, anchor_int(3)))) {
        return anchor_int(0);
    }
    AnchorVal _anc_t39 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(0)));
    AnchorVal p0 = anchor_mod(_anc_t39, anchor_int(12));
    AnchorVal _anc_t40 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(8)));
    AnchorVal p1 = anchor_mod(_anc_t40, anchor_int(12));
    AnchorVal _anc_t41 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(16)));
    AnchorVal p2 = anchor_mod(_anc_t41, anchor_int(12));
    if (_ANCH_IVAL(anchor_gt(p0, p1))) {
        AnchorVal t = p0;
        p0 = p1;
        p1 = t;
    }
    if (_ANCH_IVAL(anchor_gt(p1, p2))) {
        AnchorVal t = p1;
        p1 = p2;
        p2 = t;
    }
    if (_ANCH_IVAL(anchor_gt(p0, p1))) {
        AnchorVal t = p0;
        p0 = p1;
        p1 = t;
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(p0, p1) || !!anchor_eq(p1, p2)))) {
        return anchor_int(0);
    }
    AnchorVal i1 = anchor_sub(p1, p0);
    AnchorVal i2 = anchor_sub(p2, p1);
    AnchorVal root_pc = anchor_int(0);
    AnchorVal chord_type_str = anchor_int(0);
    AnchorVal chord_type_id = anchor_int(0);
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(4)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p0;
        chord_type_str = anchor_ext((void*)"maj");
        chord_type_id = anchor_int(1);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(5))))) {
        root_pc = p2;
        chord_type_str = anchor_ext((void*)"maj");
        chord_type_id = anchor_int(1);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(5)) && !!anchor_eq(i2, anchor_int(4))))) {
        root_pc = p1;
        chord_type_str = anchor_ext((void*)"maj");
        chord_type_id = anchor_int(1);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(4))))) {
        root_pc = p0;
        chord_type_str = anchor_ext((void*)"m");
        chord_type_id = anchor_int(2);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(4)) && !!anchor_eq(i2, anchor_int(5))))) {
        root_pc = p2;
        chord_type_str = anchor_ext((void*)"m");
        chord_type_id = anchor_int(2);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(5)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p1;
        chord_type_str = anchor_ext((void*)"m");
        chord_type_id = anchor_int(2);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p0;
        chord_type_str = anchor_ext((void*)"dim");
        chord_type_id = anchor_int(3);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(3)) && !!anchor_eq(i2, anchor_int(6))))) {
        root_pc = p2;
        chord_type_str = anchor_ext((void*)"dim");
        chord_type_id = anchor_int(3);
    }
    if (_ANCH_IVAL((AnchorVal)(!!anchor_eq(i1, anchor_int(6)) && !!anchor_eq(i2, anchor_int(3))))) {
        root_pc = p1;
        chord_type_str = anchor_ext((void*)"dim");
        chord_type_id = anchor_int(3);
    }
    if (_ANCH_IVAL(anchor_eq(chord_type_id, anchor_int(0)))) {
        return anchor_int(0);
    }
    AnchorVal _anc_t42;
    if (_ANCH_IVAL(anchor_eq(chord_type_id, anchor_int(1)))) {
        _anc_t42 = anchor_int(4);
    } else {
        _anc_t42 = anchor_int(3);
    }
    AnchorVal third_interval = _anc_t42;
    AnchorVal _anc_t43;
    if (_ANCH_IVAL(anchor_eq(chord_type_id, anchor_int(3)))) {
        _anc_t43 = anchor_int(6);
    } else {
        _anc_t43 = anchor_int(7);
    }
    AnchorVal fifth_interval = _anc_t43;
    AnchorVal third_pc = anchor_mod(anchor_add(root_pc, third_interval), anchor_int(12));
    AnchorVal fifth_pc = anchor_mod(anchor_add(root_pc, fifth_interval), anchor_int(12));
    AnchorVal _anc_t44 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_int(0)));
    AnchorVal lowest = _anc_t44;
    {
        AnchorVal k = anchor_int(1);
        while (_ANCH_IVAL(anchor_lt(k, count))) {
            AnchorVal _anc_t45 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(k, anchor_int(8))));
            if (_ANCH_IVAL(anchor_lt(_anc_t45, lowest))) {
                AnchorVal _anc_t46 = *(AnchorVal*)((char*)_ANCH_HPTR(notes) + _ANCH_IVAL(anchor_mul(k, anchor_int(8))));
                lowest = _anc_t46;
            }
            k = anchor_add(k, anchor_int(1));
        }
    }
    AnchorVal lowest_pc = anchor_mod(lowest, anchor_int(12));
    AnchorVal inversion = anchor_ext((void*)"root");
    if (_ANCH_IVAL(anchor_eq(lowest_pc, third_pc))) {
        inversion = anchor_ext((void*)"1st inversion");
    }
    if (_ANCH_IVAL(anchor_eq(lowest_pc, fifth_pc))) {
        inversion = anchor_ext((void*)"2nd inversion");
    }
    AnchorVal _anc_t47 = *(AnchorVal*)((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_mul(root_pc, anchor_int(8))));
    AnchorVal root_name = _anc_t47;
    sprintf(((char*)_anch_ptr(result)), "%s%s %s", ((char*)_anch_ptr(root_name)), ((char*)_anch_ptr(chord_type_str)), ((char*)_anch_ptr(inversion)));
    return result;
}

AnchorVal parse_args(AnchorVal argc, AnchorVal argv) {
    AnchorVal a = anchor_alloc(ANCHOR_SIZEOF_Args);
    { AnchorVal _anc_t48 = anchor_int(0);
      memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_debug, &_anc_t48, ANCHOR_SIZE_Args_debug); }
    ANCHOR_NIL;
    { AnchorVal _anc_t49 = anchor_int(0);
      memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_select_device, &_anc_t49, ANCHOR_SIZE_Args_select_device); }
    ANCHOR_NIL;
    { AnchorVal _anc_t50 = anchor_int(-1);
      memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_lesson_num, &_anc_t50, ANCHOR_SIZE_Args_lesson_num); }
    ANCHOR_NIL;
    { AnchorVal _anc_t51 = anchor_int(0);
      memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_list_lessons, &_anc_t51, ANCHOR_SIZE_Args_list_lessons); }
    ANCHOR_NIL;
    { AnchorVal _anc_t52 = anchor_int(0);
      memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_help, &_anc_t52, ANCHOR_SIZE_Args_help); }
    ANCHOR_NIL;
    {
        AnchorVal i = anchor_int(1);
        while (_ANCH_IVAL(anchor_lt(i, argc))) {
            AnchorVal _anc_t53 = *(AnchorVal*)((char*)_ANCH_HPTR(argv) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            AnchorVal arg = anchor_ext((char*)_anch_ptr(_anc_t53));
            int _anc_t54_raw = strcmp(((const char*)_anch_ptr(arg)), "-d");
            AnchorVal _anc_t54 = anchor_int((intptr_t)_anc_t54_raw);
            AnchorVal _anc_t56 = (AnchorVal)(!!anchor_eq(_anc_t54, anchor_int(0)));
            if (!_anc_t56) {
                int _anc_t55_raw = strcmp(((const char*)_anch_ptr(arg)), "--debug");
                AnchorVal _anc_t55 = anchor_int((intptr_t)_anc_t55_raw);
                _anc_t56 = (AnchorVal)(!!anchor_eq(_anc_t55, anchor_int(0)));
            }
            if (_ANCH_IVAL(_anc_t56)) {
                { AnchorVal _anc_t57 = anchor_int(1);
                  memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_debug, &_anc_t57, ANCHOR_SIZE_Args_debug); }
                ANCHOR_NIL;
            }
            int _anc_t58_raw = strcmp(((const char*)_anch_ptr(arg)), "-s");
            AnchorVal _anc_t58 = anchor_int((intptr_t)_anc_t58_raw);
            AnchorVal _anc_t60 = (AnchorVal)(!!anchor_eq(_anc_t58, anchor_int(0)));
            if (!_anc_t60) {
                int _anc_t59_raw = strcmp(((const char*)_anch_ptr(arg)), "--select-device");
                AnchorVal _anc_t59 = anchor_int((intptr_t)_anc_t59_raw);
                _anc_t60 = (AnchorVal)(!!anchor_eq(_anc_t59, anchor_int(0)));
            }
            if (_ANCH_IVAL(_anc_t60)) {
                { AnchorVal _anc_t61 = anchor_int(1);
                  memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_select_device, &_anc_t61, ANCHOR_SIZE_Args_select_device); }
                ANCHOR_NIL;
            }
            int _anc_t62_raw = strcmp(((const char*)_anch_ptr(arg)), "-l");
            AnchorVal _anc_t62 = anchor_int((intptr_t)_anc_t62_raw);
            AnchorVal _anc_t64 = (AnchorVal)(!!anchor_eq(_anc_t62, anchor_int(0)));
            if (!_anc_t64) {
                int _anc_t63_raw = strcmp(((const char*)_anch_ptr(arg)), "--list");
                AnchorVal _anc_t63 = anchor_int((intptr_t)_anc_t63_raw);
                _anc_t64 = (AnchorVal)(!!anchor_eq(_anc_t63, anchor_int(0)));
            }
            if (_ANCH_IVAL(_anc_t64)) {
                { AnchorVal _anc_t65 = anchor_int(1);
                  memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_list_lessons, &_anc_t65, ANCHOR_SIZE_Args_list_lessons); }
                ANCHOR_NIL;
            }
            int _anc_t66_raw = strcmp(((const char*)_anch_ptr(arg)), "-h");
            AnchorVal _anc_t66 = anchor_int((intptr_t)_anc_t66_raw);
            AnchorVal _anc_t68 = (AnchorVal)(!!anchor_eq(_anc_t66, anchor_int(0)));
            if (!_anc_t68) {
                int _anc_t67_raw = strcmp(((const char*)_anch_ptr(arg)), "--help");
                AnchorVal _anc_t67 = anchor_int((intptr_t)_anc_t67_raw);
                _anc_t68 = (AnchorVal)(!!anchor_eq(_anc_t67, anchor_int(0)));
            }
            if (_ANCH_IVAL(_anc_t68)) {
                { AnchorVal _anc_t69 = anchor_int(1);
                  memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_help, &_anc_t69, ANCHOR_SIZE_Args_help); }
                ANCHOR_NIL;
            }
            int _anc_t70_raw = strcmp(((const char*)_anch_ptr(arg)), "--lesson");
            AnchorVal _anc_t70 = anchor_int((intptr_t)_anc_t70_raw);
            if (_ANCH_IVAL(anchor_eq(_anc_t70, anchor_int(0)))) {
                if (_ANCH_IVAL(anchor_lt(i, anchor_sub(argc, anchor_int(1))))) {
                    i = anchor_add(i, anchor_int(1));
                    AnchorVal _anc_t71 = *(AnchorVal*)((char*)_ANCH_HPTR(argv) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
                    int _anc_t72_raw = atoi(((char*)_anch_ptr(_anc_t71)));
                    AnchorVal _anc_t72 = anchor_int((intptr_t)_anc_t72_raw);
                    { AnchorVal _anc_t73 = _anc_t72;
                      memcpy((char*)_ANCH_HPTR(a) + ANCHOR_OFFSET_Args_lesson_num, &_anc_t73, ANCHOR_SIZE_Args_lesson_num); }
                    ANCHOR_NIL;
                }
            }
            i = anchor_add(i, anchor_int(1));
        }
    }
    return a;
}

AnchorVal print_help(void) {
    printf("Usage: chordgen [options]\n\n");
    printf("MIDI chord trainer with structured lessons.\n\n");
    printf("Options:\n");
    printf("  -l, --list             List all available lessons\n");
    printf("      --lesson N         Start lesson number N directly\n");
    printf("  -s, --select-device    List MIDI input devices and choose one interactively\n");
    printf("  -d, --debug            Print raw MIDI note numbers on each key event\n");
    printf("  -h, --help             Show this help message\n");
    return anchor_int(0);
}

AnchorVal print_lesson_list(AnchorVal lessons, AnchorVal num_lessons) {
    printf("\nAvailable lessons:\n\n");
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, num_lessons))) {
            AnchorVal les = anchor_add(lessons, anchor_mul(i, anchor_int(24)));
            AnchorVal _anc_t74 = *(AnchorVal*)((char*)_ANCH_HPTR(les) + ANCHOR_OFFSET_Lesson_name);
            AnchorVal name = _anc_t74;
            AnchorVal _anc_t75 = *(AnchorVal*)((char*)_ANCH_HPTR(les) + ANCHOR_OFFSET_Lesson_num_exercises);
            AnchorVal n_ex = _anc_t75;
            printf("  \x1B[93m%d\x1B[0m. \x1B[96m%s\x1B[0m", (int)_ANCH_IVAL(anchor_add(i, anchor_int(1))), ((char*)_anch_ptr(name)));
            AnchorVal _anc_t76;
            if (_ANCH_IVAL(anchor_eq(n_ex, anchor_int(1)))) {
                _anc_t76 = anchor_ext((void*)"");
            } else {
                _anc_t76 = anchor_ext((void*)"s");
            }
            printf("  (%d exercise%s)\n", (int)_ANCH_IVAL(n_ex), ((char*)_anch_ptr(_anc_t76)));
            i = anchor_add(i, anchor_int(1));
        }
    }
    printf("\n");
    return anchor_int(0);
}

AnchorVal select_lesson_menu(AnchorVal lessons, AnchorVal num_lessons) {
    {
        char _anc_arena__anc_t77_buf[ANCHOR_DEFAULT_ARENA_CAP];
        _AnchorArena _anc_arena__anc_t77 = {_anc_arena__anc_t77_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
        _anchor_arena_top = &_anc_arena__anc_t77;
        print_lesson_list(lessons, num_lessons);
        printf("Enter lesson number: ");
        AnchorVal buf = anchor_alloc(16);
        fgets(((char*)_anch_ptr(buf)), 16, ((void*)_anch_ptr(anchor_int((intptr_t)(stdin)))));
        int _anc_t78_raw = atoi(((char*)_anch_ptr(buf)));
        AnchorVal _anc_t78 = anchor_int((intptr_t)_anc_t78_raw);
        AnchorVal n = anchor_sub(_anc_t78, anchor_int(1));
        if (_ANCH_IVAL((AnchorVal)(!!anchor_lt(n, anchor_int(0)) || !!anchor_ge(n, num_lessons)))) {
            printf("Invalid selection, defaulting to lesson 1\n");
            _anchor_arena_top = _anc_arena__anc_t77.prev;
            return anchor_int(0);
        }
        _anchor_arena_top = _anc_arena__anc_t77.prev;
        return n;
        _anchor_arena_top = _anc_arena__anc_t77.prev;
    }
    return anchor_int(0);
}

AnchorVal print_progression_line(AnchorVal prog, AnchorVal prog_len, AnchorVal note_names, AnchorVal key_id, AnchorVal chord_bufs) {
    printf("  Chords:");
    {
        AnchorVal i = anchor_int(0);
        while (_ANCH_IVAL(anchor_lt(i, prog_len))) {
            AnchorVal _anc_t79 = *(AnchorVal*)((char*)_ANCH_HPTR(prog) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            AnchorVal degree = _anc_t79;
            AnchorVal _anc_t80 = *(AnchorVal*)((char*)_ANCH_HPTR(chord_bufs) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
            printf(" \x1B[93m%s\x1B[0m: \x1B[96m%s\x1B[0m", ((char*)_anch_ptr(degree_roman(degree))), ((char*)_anch_ptr(_anc_t80)));
            if (_ANCH_IVAL(anchor_lt(i, anchor_sub(prog_len, anchor_int(1))))) {
                printf("  ");
            }
            i = anchor_add(i, anchor_int(1));
        }
    }
    printf("\n");
    return anchor_int(0);
}

AnchorVal challenge(AnchorVal degree, AnchorVal target, AnchorVal midi, AnchorVal event, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal note_names, AnchorVal debug) {
    printf("Play \x1B[93m%s\x1B[0m: \x1B[96m%s\x1B[0m\n", ((char*)_anch_ptr(degree_roman(degree))), ((char*)_anch_ptr(target)));
    { AnchorVal _anc_t81 = anchor_int(0);
      memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t81, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    { AnchorVal _anc_t82 = anchor_int(0);
      memcpy((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t82, sizeof(AnchorVal)); }
    ANCHOR_NIL;
    read_chord(midi, event, active_notes, note_count, last_notes, last_count, debug);
    AnchorVal id_buf = anchor_alloc(32);
    AnchorVal _anc_t83 = *(AnchorVal*)((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)));
    AnchorVal identified = identify_chord(last_notes, _anc_t83, note_names, id_buf);
    AnchorVal _anc_t85 = (AnchorVal)(!!identified);
    if (_anc_t85) {
        int _anc_t84_raw = strcmp(((char*)_anch_ptr(identified)), ((char*)_anch_ptr(target)));
        AnchorVal _anc_t84 = anchor_int((intptr_t)_anc_t84_raw);
        _anc_t85 = (AnchorVal)(!!anchor_eq(_anc_t84, anchor_int(0)));
    }
    if (_ANCH_IVAL(_anc_t85)) {
        printf("\x1B[92mCorrect!\x1B[0m\n");
        return anchor_int(1);
    }
    if (_ANCH_IVAL(identified)) {
        printf("That's \x1B[91m%s\x1B[0m, try again\n", ((char*)_anch_ptr(identified)));
    } else {
        printf("\x1B[93mUnknown chord\x1B[0m, try again\n");
    }
    return anchor_int(0);
}

AnchorVal run_exercise(AnchorVal ex, AnchorVal midi, AnchorVal event, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal note_names, AnchorVal debug) {
    AnchorVal _anc_t86 = *(AnchorVal*)((char*)_ANCH_HPTR(ex) + ANCHOR_OFFSET_Exercise_num_keys);
    AnchorVal num_keys = _anc_t86;
    AnchorVal _anc_t87 = *(AnchorVal*)((char*)_ANCH_HPTR(ex) + ANCHOR_OFFSET_Exercise_keys);
    AnchorVal keys = _anc_t87;
    AnchorVal _anc_t88 = *(AnchorVal*)((char*)_ANCH_HPTR(ex) + ANCHOR_OFFSET_Exercise_prog_len);
    AnchorVal prog_len = _anc_t88;
    AnchorVal _anc_t89 = *(AnchorVal*)((char*)_ANCH_HPTR(ex) + ANCHOR_OFFSET_Exercise_progression);
    AnchorVal prog = _anc_t89;
    AnchorVal _anc_t90 = *(AnchorVal*)((char*)_ANCH_HPTR(ex) + ANCHOR_OFFSET_Exercise_reps);
    AnchorVal reps = _anc_t90;
    AnchorVal random_mode = anchor_eq(prog_len, anchor_int(0));
    int _anc_t91_raw = rand();
    AnchorVal _anc_t91 = anchor_int((intptr_t)_anc_t91_raw);
    AnchorVal key_idx = anchor_mod(_anc_t91, num_keys);
    AnchorVal _anc_t92 = *(AnchorVal*)((char*)_ANCH_HPTR(keys) + _ANCH_IVAL(anchor_mul(key_idx, anchor_int(8))));
    AnchorVal key_id = _anc_t92;
    AnchorVal key_buf = anchor_alloc(32);
    key_name(key_id, note_names, key_buf);
    if (_ANCH_IVAL(random_mode)) {
        printf("\nKey: \x1B[96m%s\x1B[0m  |  \x1B[93mRandom chords\x1B[0m\n\n", ((char*)_anch_ptr(key_buf)));
        AnchorVal rounds_done = anchor_int(0);
        while (_ANCH_IVAL((AnchorVal)(!!anchor_eq(reps, anchor_int(-1)) || !!anchor_lt(rounds_done, reps)))) {
            {
                size_t _anc_cp__anc_t93_used = _anchor_arena_top->used;
                size_t _anc_cp__anc_t93_prev = _anchor_arena_top->checkpoint;
                _anchor_arena_top->checkpoint = _anchor_arena_top->used;
                int _anc_t94_raw = rand();
                AnchorVal _anc_t94 = anchor_int((intptr_t)_anc_t94_raw);
                AnchorVal degree = anchor_add(anchor_int(1), anchor_mod(_anc_t94, anchor_int(7)));
                int _anc_t95_raw = rand();
                AnchorVal _anc_t95 = anchor_int((intptr_t)_anc_t95_raw);
                AnchorVal inv = anchor_mod(_anc_t95, anchor_int(3));
                AnchorVal target = anchor_alloc(32);
                chord_name_for_degree(key_id, degree, inv, note_names, target);
                AnchorVal done = anchor_int(0);
                while (_ANCH_IVAL(anchor_not(done))) {
                    if (_ANCH_IVAL(challenge(degree, target, midi, event, active_notes, note_count, last_notes, last_count, note_names, debug))) {
                        done = anchor_int(1);
                    }
                }
                if (_ANCH_IVAL(anchor_ne(reps, anchor_int(-1)))) {
                    rounds_done = anchor_add(rounds_done, anchor_int(1));
                }
                _anchor_arena_top->used = _anc_cp__anc_t93_used;
                _anchor_arena_top->checkpoint = _anc_cp__anc_t93_prev;
            }
        }
    } else {
        AnchorVal inv_buf = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(prog_len, anchor_int(8))));
        {
            AnchorVal i = anchor_int(0);
            while (_ANCH_IVAL(anchor_lt(i, prog_len))) {
                int _anc_t96_raw = rand();
                AnchorVal _anc_t96 = anchor_int((intptr_t)_anc_t96_raw);
                { AnchorVal _anc_t97 = anchor_mod(_anc_t96, anchor_int(3));
                  memcpy((char*)_ANCH_HPTR(inv_buf) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), &_anc_t97, sizeof(AnchorVal)); }
                ANCHOR_NIL;
                i = anchor_add(i, anchor_int(1));
            }
        }
        AnchorVal chord_bufs = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(prog_len, anchor_int(8))));
        {
            AnchorVal i = anchor_int(0);
            while (_ANCH_IVAL(anchor_lt(i, prog_len))) {
                AnchorVal cstr = anchor_alloc(32);
                AnchorVal _anc_t98 = *(AnchorVal*)((char*)_ANCH_HPTR(inv_buf) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
                AnchorVal _anc_t99 = *(AnchorVal*)((char*)_ANCH_HPTR(prog) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
                chord_name_for_degree(key_id, _anc_t99, _anc_t98, note_names, cstr);
                { AnchorVal _anc_t100 = cstr;
                  memcpy((char*)_ANCH_HPTR(chord_bufs) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))), &_anc_t100, sizeof(AnchorVal)); }
                ANCHOR_NIL;
                i = anchor_add(i, anchor_int(1));
            }
        }
        printf("\nKey: \x1B[96m%s\x1B[0m  |  ", ((char*)_anch_ptr(key_buf)));
        {
            AnchorVal i = anchor_int(0);
            while (_ANCH_IVAL(anchor_lt(i, prog_len))) {
                AnchorVal _anc_t101 = *(AnchorVal*)((char*)_ANCH_HPTR(prog) + _ANCH_IVAL(anchor_mul(i, anchor_int(8))));
                printf("\x1B[93m%s\x1B[0m", ((char*)_anch_ptr(degree_roman(_anc_t101))));
                if (_ANCH_IVAL(anchor_lt(i, anchor_sub(prog_len, anchor_int(1))))) {
                    printf(" - ");
                }
                i = anchor_add(i, anchor_int(1));
            }
        }
        printf("\n");
        print_progression_line(prog, prog_len, note_names, key_id, chord_bufs);
        printf("\n");
        AnchorVal rounds_done = anchor_int(0);
        while (_ANCH_IVAL((AnchorVal)(!!anchor_eq(reps, anchor_int(-1)) || !!anchor_lt(rounds_done, reps)))) {
            AnchorVal chord_idx = anchor_int(0);
            while (_ANCH_IVAL(anchor_lt(chord_idx, prog_len))) {
                {
                    size_t _anc_cp__anc_t102_used = _anchor_arena_top->used;
                    size_t _anc_cp__anc_t102_prev = _anchor_arena_top->checkpoint;
                    _anchor_arena_top->checkpoint = _anchor_arena_top->used;
                    AnchorVal _anc_t103 = *(AnchorVal*)((char*)_ANCH_HPTR(prog) + _ANCH_IVAL(anchor_mul(chord_idx, anchor_int(8))));
                    AnchorVal degree = _anc_t103;
                    AnchorVal _anc_t104 = *(AnchorVal*)((char*)_ANCH_HPTR(chord_bufs) + _ANCH_IVAL(anchor_mul(chord_idx, anchor_int(8))));
                    AnchorVal target = _anc_t104;
                    if (_ANCH_IVAL(challenge(degree, target, midi, event, active_notes, note_count, last_notes, last_count, note_names, debug))) {
                        chord_idx = anchor_add(chord_idx, anchor_int(1));
                    }
                    _anchor_arena_top->used = _anc_cp__anc_t102_used;
                    _anchor_arena_top->checkpoint = _anc_cp__anc_t102_prev;
                }
            }
            if (_ANCH_IVAL(anchor_ne(reps, anchor_int(-1)))) {
                rounds_done = anchor_add(rounds_done, anchor_int(1));
            }
        }
    }
    return anchor_int(0);
}

AnchorVal run_lesson(AnchorVal lesson, AnchorVal midi, AnchorVal event, AnchorVal active_notes, AnchorVal note_count, AnchorVal last_notes, AnchorVal last_count, AnchorVal note_names, AnchorVal debug) {
    AnchorVal _anc_t105 = *(AnchorVal*)((char*)_ANCH_HPTR(lesson) + ANCHOR_OFFSET_Lesson_num_exercises);
    AnchorVal n_ex = _anc_t105;
    AnchorVal _anc_t106 = *(AnchorVal*)((char*)_ANCH_HPTR(lesson) + ANCHOR_OFFSET_Lesson_exercises);
    AnchorVal exptrs = _anc_t106;
    AnchorVal ex_idx = anchor_int(0);
    while (_ANCH_IVAL(anchor_int(1))) {
        AnchorVal _anc_t107 = *(AnchorVal*)((char*)_ANCH_HPTR(exptrs) + _ANCH_IVAL(anchor_mul(ex_idx, anchor_int(8))));
        AnchorVal ex = _anc_t107;
        run_exercise(ex, midi, event, active_notes, note_count, last_notes, last_count, note_names, debug);
        ex_idx = anchor_mod(anchor_add(ex_idx, anchor_int(1)), n_ex);
        if (_ANCH_IVAL(anchor_eq(ex_idx, anchor_int(0)))) {
            printf("\n\x1B[92mLesson complete! Starting over...\x1B[0m\n");
        } else {
            printf("\n\x1B[92mExercise complete! Next exercise...\x1B[0m\n");
        }
    }
    return anchor_int(0);
}

int main(int _argc_raw, char** _argv_raw) {
    AnchorVal argc = anchor_int(_argc_raw);
    AnchorVal argv = anchor_ext((void*)_argv_raw);
    {
        char _anc_arena__anc_t108_buf[ANCHOR_DEFAULT_ARENA_CAP];
        _AnchorArena _anc_arena__anc_t108 = {_anc_arena__anc_t108_buf, ANCHOR_DEFAULT_ARENA_CAP, 0, 0, _anchor_arena_top};
        _anchor_arena_top = &_anc_arena__anc_t108;
        SetConsoleOutputCP(65001);
        int _anc_t109_raw = _fileno(((void*)_anch_ptr(anchor_int((intptr_t)(stdout)))));
        AnchorVal _anc_t109 = anchor_int((intptr_t)_anc_t109_raw);
        _setmode((int)_ANCH_IVAL(_anc_t109), 32768);
        AnchorVal args = parse_args(argc, argv);
        AnchorVal _anc_t110 = *(AnchorVal*)((char*)_ANCH_HPTR(args) + ANCHOR_OFFSET_Args_debug);
        AnchorVal debug = _anc_t110;
        AnchorVal _anc_t111 = *(AnchorVal*)((char*)_ANCH_HPTR(args) + ANCHOR_OFFSET_Args_select_device);
        AnchorVal select_device = _anc_t111;
        AnchorVal _anc_t112 = *(AnchorVal*)((char*)_ANCH_HPTR(args) + ANCHOR_OFFSET_Args_lesson_num);
        AnchorVal lesson_num = _anc_t112;
        AnchorVal _anc_t113 = *(AnchorVal*)((char*)_ANCH_HPTR(args) + ANCHOR_OFFSET_Args_list_lessons);
        AnchorVal list_lessons = _anc_t113;
        AnchorVal _anc_t114 = *(AnchorVal*)((char*)_ANCH_HPTR(args) + ANCHOR_OFFSET_Args_help);
        AnchorVal help = _anc_t114;
        if (_ANCH_IVAL(help)) {
            print_help();
            _anchor_arena_top = _anc_arena__anc_t108.prev;
            return 0;
        }
        AnchorVal note_names = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(12), anchor_int(8))));
        { AnchorVal _anc_t115 = anchor_ext((void*)"C");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(0)), &_anc_t115, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t116 = anchor_ext((void*)"C♯");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(8)), &_anc_t116, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t117 = anchor_ext((void*)"D");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(16)), &_anc_t117, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t118 = anchor_ext((void*)"E♭");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(24)), &_anc_t118, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t119 = anchor_ext((void*)"E");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(32)), &_anc_t119, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t120 = anchor_ext((void*)"F");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(40)), &_anc_t120, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t121 = anchor_ext((void*)"F♯");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(48)), &_anc_t121, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t122 = anchor_ext((void*)"G");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(56)), &_anc_t122, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t123 = anchor_ext((void*)"A♭");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(64)), &_anc_t123, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t124 = anchor_ext((void*)"A");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(72)), &_anc_t124, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t125 = anchor_ext((void*)"B♭");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(80)), &_anc_t125, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t126 = anchor_ext((void*)"B");
          memcpy((char*)_ANCH_HPTR(note_names) + _ANCH_IVAL(anchor_int(88)), &_anc_t126, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal all_lessons = anchor_alloc(120);
        AnchorVal num_all_lessons = anchor_int(5);
        AnchorVal _ks0_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t127 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ks0_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t127, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ex0_anc_192 = anchor_alloc(ANCHOR_SIZEOF_Exercise);
        { AnchorVal _anc_t128 = _ks0_anc_192;
          memcpy((char*)_ANCH_HPTR(_ex0_anc_192) + ANCHOR_OFFSET_Exercise_keys, &_anc_t128, ANCHOR_SIZE_Exercise_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t129 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(_ex0_anc_192) + ANCHOR_OFFSET_Exercise_num_keys, &_anc_t129, ANCHOR_SIZE_Exercise_num_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t130 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex0_anc_192) + ANCHOR_OFFSET_Exercise_progression, &_anc_t130, ANCHOR_SIZE_Exercise_progression); }
        ANCHOR_NIL;
        { AnchorVal _anc_t131 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex0_anc_192) + ANCHOR_OFFSET_Exercise_prog_len, &_anc_t131, ANCHOR_SIZE_Exercise_prog_len); }
        ANCHOR_NIL;
        { AnchorVal _anc_t132 = anchor_int(-1);
          memcpy((char*)_ANCH_HPTR(_ex0_anc_192) + ANCHOR_OFFSET_Exercise_reps, &_anc_t132, ANCHOR_SIZE_Exercise_reps); }
        ANCHOR_NIL;
        AnchorVal _ep0_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t133 = _ex0_anc_192;
          memcpy((char*)_ANCH_HPTR(_ep0_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t133, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t134 = anchor_ext((void*)"C major Triads");
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(0)), &_anc_t134, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t135 = _ep0_anc_192;
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(8)), &_anc_t135, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t136 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(16)), &_anc_t136, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ks100_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t137 = anchor_int(2);
          memcpy((char*)_ANCH_HPTR(_ks100_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t137, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ex100_anc_192 = anchor_alloc(ANCHOR_SIZEOF_Exercise);
        { AnchorVal _anc_t138 = _ks100_anc_192;
          memcpy((char*)_ANCH_HPTR(_ex100_anc_192) + ANCHOR_OFFSET_Exercise_keys, &_anc_t138, ANCHOR_SIZE_Exercise_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t139 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(_ex100_anc_192) + ANCHOR_OFFSET_Exercise_num_keys, &_anc_t139, ANCHOR_SIZE_Exercise_num_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t140 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex100_anc_192) + ANCHOR_OFFSET_Exercise_progression, &_anc_t140, ANCHOR_SIZE_Exercise_progression); }
        ANCHOR_NIL;
        { AnchorVal _anc_t141 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex100_anc_192) + ANCHOR_OFFSET_Exercise_prog_len, &_anc_t141, ANCHOR_SIZE_Exercise_prog_len); }
        ANCHOR_NIL;
        { AnchorVal _anc_t142 = anchor_int(-1);
          memcpy((char*)_ANCH_HPTR(_ex100_anc_192) + ANCHOR_OFFSET_Exercise_reps, &_anc_t142, ANCHOR_SIZE_Exercise_reps); }
        ANCHOR_NIL;
        AnchorVal _ep100_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t143 = _ex100_anc_192;
          memcpy((char*)_ANCH_HPTR(_ep100_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t143, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t144 = anchor_ext((void*)"D major Triads");
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(24)), &_anc_t144, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t145 = _ep100_anc_192;
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(32)), &_anc_t145, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t146 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(40)), &_anc_t146, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ks200_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t147 = anchor_int(5);
          memcpy((char*)_ANCH_HPTR(_ks200_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t147, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ex200_anc_192 = anchor_alloc(ANCHOR_SIZEOF_Exercise);
        { AnchorVal _anc_t148 = _ks200_anc_192;
          memcpy((char*)_ANCH_HPTR(_ex200_anc_192) + ANCHOR_OFFSET_Exercise_keys, &_anc_t148, ANCHOR_SIZE_Exercise_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t149 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(_ex200_anc_192) + ANCHOR_OFFSET_Exercise_num_keys, &_anc_t149, ANCHOR_SIZE_Exercise_num_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t150 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex200_anc_192) + ANCHOR_OFFSET_Exercise_progression, &_anc_t150, ANCHOR_SIZE_Exercise_progression); }
        ANCHOR_NIL;
        { AnchorVal _anc_t151 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex200_anc_192) + ANCHOR_OFFSET_Exercise_prog_len, &_anc_t151, ANCHOR_SIZE_Exercise_prog_len); }
        ANCHOR_NIL;
        { AnchorVal _anc_t152 = anchor_int(-1);
          memcpy((char*)_ANCH_HPTR(_ex200_anc_192) + ANCHOR_OFFSET_Exercise_reps, &_anc_t152, ANCHOR_SIZE_Exercise_reps); }
        ANCHOR_NIL;
        AnchorVal _ep200_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t153 = _ex200_anc_192;
          memcpy((char*)_ANCH_HPTR(_ep200_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t153, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t154 = anchor_ext((void*)"F major Triads");
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(48)), &_anc_t154, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t155 = _ep200_anc_192;
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(56)), &_anc_t155, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t156 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(64)), &_anc_t156, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ks300_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t157 = anchor_int(10);
          memcpy((char*)_ANCH_HPTR(_ks300_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t157, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ex300_anc_192 = anchor_alloc(ANCHOR_SIZEOF_Exercise);
        { AnchorVal _anc_t158 = _ks300_anc_192;
          memcpy((char*)_ANCH_HPTR(_ex300_anc_192) + ANCHOR_OFFSET_Exercise_keys, &_anc_t158, ANCHOR_SIZE_Exercise_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t159 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(_ex300_anc_192) + ANCHOR_OFFSET_Exercise_num_keys, &_anc_t159, ANCHOR_SIZE_Exercise_num_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t160 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex300_anc_192) + ANCHOR_OFFSET_Exercise_progression, &_anc_t160, ANCHOR_SIZE_Exercise_progression); }
        ANCHOR_NIL;
        { AnchorVal _anc_t161 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ex300_anc_192) + ANCHOR_OFFSET_Exercise_prog_len, &_anc_t161, ANCHOR_SIZE_Exercise_prog_len); }
        ANCHOR_NIL;
        { AnchorVal _anc_t162 = anchor_int(-1);
          memcpy((char*)_ANCH_HPTR(_ex300_anc_192) + ANCHOR_OFFSET_Exercise_reps, &_anc_t162, ANCHOR_SIZE_Exercise_reps); }
        ANCHOR_NIL;
        AnchorVal _ep300_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t163 = _ex300_anc_192;
          memcpy((char*)_ANCH_HPTR(_ep300_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t163, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t164 = anchor_ext((void*)"B♭ major Triads");
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(72)), &_anc_t164, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t165 = _ep300_anc_192;
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(80)), &_anc_t165, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t166 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(88)), &_anc_t166, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ks400_anc_192 = anchor_alloc(32);
        { AnchorVal _anc_t167 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(_ks400_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t167, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t168 = anchor_int(2);
          memcpy((char*)_ANCH_HPTR(_ks400_anc_192) + _ANCH_IVAL(anchor_int(8)), &_anc_t168, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t169 = anchor_int(5);
          memcpy((char*)_ANCH_HPTR(_ks400_anc_192) + _ANCH_IVAL(anchor_int(16)), &_anc_t169, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t170 = anchor_int(10);
          memcpy((char*)_ANCH_HPTR(_ks400_anc_192) + _ANCH_IVAL(anchor_int(24)), &_anc_t170, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _pr400_anc_192 = anchor_alloc(32);
        { AnchorVal _anc_t171 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(_pr400_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t171, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t172 = anchor_int(5);
          memcpy((char*)_ANCH_HPTR(_pr400_anc_192) + _ANCH_IVAL(anchor_int(8)), &_anc_t172, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t173 = anchor_int(6);
          memcpy((char*)_ANCH_HPTR(_pr400_anc_192) + _ANCH_IVAL(anchor_int(16)), &_anc_t173, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t174 = anchor_int(4);
          memcpy((char*)_ANCH_HPTR(_pr400_anc_192) + _ANCH_IVAL(anchor_int(24)), &_anc_t174, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal _ex400_anc_192 = anchor_alloc(ANCHOR_SIZEOF_Exercise);
        { AnchorVal _anc_t175 = _ks400_anc_192;
          memcpy((char*)_ANCH_HPTR(_ex400_anc_192) + ANCHOR_OFFSET_Exercise_keys, &_anc_t175, ANCHOR_SIZE_Exercise_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t176 = anchor_int(4);
          memcpy((char*)_ANCH_HPTR(_ex400_anc_192) + ANCHOR_OFFSET_Exercise_num_keys, &_anc_t176, ANCHOR_SIZE_Exercise_num_keys); }
        ANCHOR_NIL;
        { AnchorVal _anc_t177 = _pr400_anc_192;
          memcpy((char*)_ANCH_HPTR(_ex400_anc_192) + ANCHOR_OFFSET_Exercise_progression, &_anc_t177, ANCHOR_SIZE_Exercise_progression); }
        ANCHOR_NIL;
        { AnchorVal _anc_t178 = anchor_int(4);
          memcpy((char*)_ANCH_HPTR(_ex400_anc_192) + ANCHOR_OFFSET_Exercise_prog_len, &_anc_t178, ANCHOR_SIZE_Exercise_prog_len); }
        ANCHOR_NIL;
        { AnchorVal _anc_t179 = anchor_int(10);
          memcpy((char*)_ANCH_HPTR(_ex400_anc_192) + ANCHOR_OFFSET_Exercise_reps, &_anc_t179, ANCHOR_SIZE_Exercise_reps); }
        ANCHOR_NIL;
        AnchorVal _ep400_anc_192 = anchor_alloc(8);
        { AnchorVal _anc_t180 = _ex400_anc_192;
          memcpy((char*)_ANCH_HPTR(_ep400_anc_192) + _ANCH_IVAL(anchor_int(0)), &_anc_t180, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t181 = anchor_ext((void*)"I - V - VI - IV (C, D, F, B♭)");
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(96)), &_anc_t181, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t182 = _ep400_anc_192;
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(104)), &_anc_t182, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        { AnchorVal _anc_t183 = anchor_int(1);
          memcpy((char*)_ANCH_HPTR(all_lessons) + _ANCH_IVAL(anchor_int(112)), &_anc_t183, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        if (_ANCH_IVAL(list_lessons)) {
            print_lesson_list(all_lessons, num_all_lessons);
            _anchor_arena_top = _anc_arena__anc_t108.prev;
            return 0;
        }
        AnchorVal input_buffer_size = anchor_int(128);
        AnchorVal midi = anchor_alloc(8);
        AnchorVal event = anchor_alloc(256);
        AnchorVal active_notes = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(16), anchor_int(8))));
        AnchorVal note_count = anchor_alloc(8);
        { AnchorVal _anc_t184 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(note_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t184, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        AnchorVal last_notes = anchor_alloc((size_t)_ANCH_IVAL(anchor_mul(anchor_int(16), anchor_int(8))));
        AnchorVal last_count = anchor_alloc(8);
        { AnchorVal _anc_t185 = anchor_int(0);
          memcpy((char*)_ANCH_HPTR(last_count) + _ANCH_IVAL(anchor_int(0)), &_anc_t185, sizeof(AnchorVal)); }
        ANCHOR_NIL;
        long _anc_t186_raw = time((void*)0);
        AnchorVal _anc_t186 = anchor_int((intptr_t)_anc_t186_raw);
        srand((unsigned int)_ANCH_IVAL(_anc_t186));
        int _anc_t187_raw = Pm_Initialize();
        AnchorVal _anc_t187 = anchor_int((intptr_t)_anc_t187_raw);
        if (_ANCH_IVAL(anchor_ne(_anc_t187, anchor_int(0)))) {
            printf("Failed to initialize PortMidi\n");
            _anchor_arena_top = _anc_arena__anc_t108.prev;
            return 1;
        }
        AnchorVal device_id = find_midi_input_device(select_device);
        if (_ANCH_IVAL(anchor_eq(device_id, anchor_int(-1)))) {
            printf("No MIDI input device found\n");
            _anchor_arena_top = _anc_arena__anc_t108.prev;
            return 1;
        }
        int _anc_t188_raw = Pm_OpenInput(((void*)_anch_ptr(midi)), (int)_ANCH_IVAL(device_id), 0, (int)_ANCH_IVAL(input_buffer_size), (void*)0, 0);
        AnchorVal _anc_t188 = anchor_int((intptr_t)_anc_t188_raw);
        if (_ANCH_IVAL(anchor_ne(_anc_t188, anchor_int(0)))) {
            printf("Failed to open MIDI input\n");
            _anchor_arena_top = _anc_arena__anc_t108.prev;
            return 1;
        }
        AnchorVal _anc_t189;
        if (_ANCH_IVAL(anchor_ne(lesson_num, anchor_int(-1)))) {
            AnchorVal _anc_t190;
            {
                if (_ANCH_IVAL((AnchorVal)(!!anchor_lt(lesson_num, anchor_int(1)) || !!anchor_gt(lesson_num, num_all_lessons)))) {
                    printf("Lesson %d does not exist\n", (int)_ANCH_IVAL(lesson_num));
                    _anchor_arena_top = _anc_arena__anc_t108.prev;
                    return 1;
                }
                _anc_t190 = anchor_sub(lesson_num, anchor_int(1));
            }
            _anc_t189 = _anc_t190;
        } else {
            _anc_t189 = select_lesson_menu(all_lessons, num_all_lessons);
        }
        AnchorVal chosen_idx = _anc_t189;
        AnchorVal chosen_lesson = anchor_add(all_lessons, anchor_mul(chosen_idx, anchor_int(24)));
        AnchorVal _anc_t191 = *(AnchorVal*)((char*)_ANCH_HPTR(chosen_lesson) + ANCHOR_OFFSET_Lesson_name);
        AnchorVal lesson_name = _anc_t191;
        printf("\n\x1B[96mStarting: %s\x1B[0m\n", ((char*)_anch_ptr(lesson_name)));
        run_lesson(chosen_lesson, midi, event, active_notes, note_count, last_notes, last_count, note_names, debug);
        _anchor_arena_top = _anc_arena__anc_t108.prev;
    }
    return 0;
}