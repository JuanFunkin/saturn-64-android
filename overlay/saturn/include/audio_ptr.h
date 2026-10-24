#ifndef AUDIO_PTR_H
#define AUDIO_PTR_H

// Saturn's audio data files (sound_data.ctl, sequences.bin...) are extracted from the ROM in the layout used by
// 64-bit hosts: every pointer member of the audio structs occupies 8 bytes. A 32-bit host (armeabi-v7a) would read
// them with 4-byte pointers and crash. On 32-bit hosts every such member is therefore padded to 8 bytes with an
// anonymous union, so the structs have exactly the same size and offsets as on 64-bit.
#include <stdint.h>

#if UINTPTR_MAX == 0xFFFFFFFFu
typedef uint64_t audio_u64 __attribute__((aligned(8)));
#define AUDIO_PTR(type, name) union { type name; audio_u64 name##_pad; }
#define AUDIO_HOST_32 1
#else
#define AUDIO_PTR(type, name) type name
#define AUDIO_HOST_32 0
#endif

// The in-memory audio structs always have the 64-bit size (see above), so the audio heaps need the "64-bit" size on
// 32-bit hosts too. (Same value as DOUBLE_SIZE_ON_64_BIT on 64-bit hosts.)
#define AUDIO_DOUBLE_SIZE(size) ((size) * 2)

#endif
