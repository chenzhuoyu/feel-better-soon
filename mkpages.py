#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import mimetypes

from typing import Iterator

def walkdir(path: str) -> Iterator[str]:
    for name in os.listdir(path):
        fname = os.path.join(path, name)
        if os.path.isdir(fname):
            yield from walkdir(fname)
        else:
            yield fname

root = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'pages')
lines = []
lines.append('#ifndef __PAGES_H__')
lines.append('#define __PAGES_H__')
lines.append('')
lines.append('#include <stddef.h>')
lines.append('#include <stdint.h>')
lines.append('#include <sys/pgmspace.h>')
lines.append('')

for path in walkdir(root):
    rel = os.path.relpath(path, root)
    name = re.sub('[^A-Za-z0-9_]', '_', rel)
    mtype, _ = mimetypes.guess_type(rel, strict = False)

    if mtype is None:
        mtype = 'application/octet-stream'

    with open(path, 'rb') as fp:
        buf = fp.read()

    data = b''
    data += b'HTTP/1.1 200 OK\r\n'
    data += b'Server: feel-better-soon/1.0\r\n'
    data += b'Content-Type: %s\r\n' % mtype.encode('utf-8')
    data += b'Content-Length: %d\r\n' % len(buf)
    data += b'\r\n'
    data += buf

    lines.append('// File: ' + rel)
    lines.append('static const size_t SIZE_%s = %d;' % (name, len(data)))
    lines.append('static const uint8_t DATA_%s[] PROGMEM = {' % name);

    while data:
        buf, data = data[:16], data[16:]
        lines.append('    ' + ''.join('0x%02x, ' % v for v in buf).strip())

    lines.append('};')
    lines.append('')

with open('pages.h', 'w') as fp:
    lines.append('#endif')
    fp.write('\n'.join(lines))