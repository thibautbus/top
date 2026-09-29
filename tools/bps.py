"""Applying a BPS patch in memory for the offline tools, its three CRC32s checked (the engine's own is engine/rom/bps.c)."""
import collections
import hashlib
import struct
import zlib


def number(data, cursor):
    value, shift = 0, 1
    while True:
        byte = data[cursor]
        cursor += 1
        value += (byte & 127) * shift
        if byte & 128:
            return value, cursor
        shift <<= 7
        value += shift


def patch_report(source, target, patch):
    if patch[:4] != b"BPS1":
        raise ValueError("not a BPS1 patch")
    source_size, pos = number(patch, 4)
    target_size, pos = number(patch, pos)
    metadata_size, pos = number(patch, pos)
    pos += metadata_size
    if source_size != len(source):
        raise ValueError("BPS source size does not match the supplied ROM")
    output = bytearray()
    source_relative = target_relative = 0
    counts, lengths = collections.Counter(), collections.Counter()
    names = ("source_read", "target_read", "source_copy", "target_copy")
    while pos < len(patch) - 12:
        action, pos = number(patch, pos)
        kind, length = action & 3, (action >> 2) + 1
        counts[names[kind]] += 1
        lengths[names[kind]] += length
        if kind == 0:
            output.extend(source[len(output):len(output) + length])
        elif kind == 1:
            output.extend(patch[pos:pos + length])
            pos += length
        else:
            delta, pos = number(patch, pos)
            delta = -(delta >> 1) if delta & 1 else delta >> 1
            if kind == 2:
                source_relative += delta
                output.extend(source[source_relative:source_relative + length])
                source_relative += length
            else:
                target_relative += delta
                for _ in range(length):
                    output.append(output[target_relative])
                    target_relative += 1
    checksums = struct.unpack("<III", patch[-12:])
    if len(output) != target_size or checksums != (
            zlib.crc32(source), zlib.crc32(output), zlib.crc32(patch[:-4])):
        raise ValueError("BPS size or source/target/patch CRC verification failed")
    return output, dict(source_size=source_size, target_size=target_size,
                metadata_size=metadata_size, crc32_verified=True,
                supplied_target_exact=output == target if target is not None else None,
                supplied_target_different_bytes=(sum(a != b for a, b in zip(output, target))
                                                + abs(len(output) - len(target))) if target is not None else None,
                patched_sha1=hashlib.sha1(output).hexdigest(),
                operations=counts, output_bytes=lengths)
