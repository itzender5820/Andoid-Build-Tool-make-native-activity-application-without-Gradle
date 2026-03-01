#!/usr/bin/env python3
"""
generate_tiny_dex.py
Generates a minimal valid DEX file (classes.dex) that satisfies the Android
Runtime's requirement for at least one DEX in an APK, while doing nothing
(no Java classes, no code — pure NativeActivity takes over immediately).

Run once:
    python3 generate_tiny_dex.py

The output classes.dex is committed to runtime/tiny_dex/ and bundled into
every APK by the abt packager. Size: ~112 bytes.
"""

import struct
import hashlib
import zlib
import os

def write_uleb128(value: int) -> bytes:
    result = bytearray()
    while value > 0x7f:
        result.append((value & 0x7f) | 0x80)
        value >>= 7
    result.append(value & 0x7f)
    return bytes(result)

def build_tiny_dex() -> bytes:
    """
    Builds a minimal DEX 035 file with:
      - 0 class definitions
      - 0 string ids
      - 0 type ids
      - 0 proto ids
      - 0 field ids
      - 0 method ids
    The header checksum and SHA-1 are computed after assembly.
    """
    # DEX header is 112 bytes
    HEADER_SIZE = 0x70

    magic       = b"dex\n035\x00"
    checksum    = b"\x00" * 4      # placeholder
    sha1        = b"\x00" * 20     # placeholder
    file_size   = struct.pack("<I", HEADER_SIZE)
    header_size = struct.pack("<I", HEADER_SIZE)
    endian_tag  = struct.pack("<I", 0x12345678)  # ENDIAN_CONSTANT

    # All section counts and offsets are 0 (empty DEX)
    link_size   = struct.pack("<I", 0)
    link_off    = struct.pack("<I", 0)
    map_off     = struct.pack("<I", 0)
    string_ids  = struct.pack("<II", 0, 0)  # size, offset
    type_ids    = struct.pack("<II", 0, 0)
    proto_ids   = struct.pack("<II", 0, 0)
    field_ids   = struct.pack("<II", 0, 0)
    method_ids  = struct.pack("<II", 0, 0)
    class_defs  = struct.pack("<II", 0, 0)
    data        = struct.pack("<II", 0, HEADER_SIZE)  # size, offset

    header = (magic + checksum + sha1 + file_size + header_size
              + endian_tag + link_size + link_off + map_off
              + string_ids + type_ids + proto_ids + field_ids
              + method_ids + class_defs + data)

    assert len(header) == HEADER_SIZE, f"Header size mismatch: {len(header)}"

    # Compute SHA-1 of bytes 32..end
    sha1_hash = hashlib.sha1(header[32:]).digest()
    header = header[:12] + sha1_hash + header[32:]

    # Compute Adler-32 checksum of bytes 12..end
    adler = zlib.adler32(header[12:]) & 0xFFFFFFFF
    header = header[:8] + struct.pack("<I", adler) + header[12:]

    return header


if __name__ == "__main__":
    out_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(out_dir, "classes.dex")

    dex = build_tiny_dex()

    with open(out_path, "wb") as f:
        f.write(dex)

    print(f"Written {len(dex)} bytes → {out_path}")
    print("SHA-1:", hashlib.sha1(dex).hexdigest())
