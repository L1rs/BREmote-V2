#!/usr/bin/env python3
"""
Checks that the confStruct in the firmware still matches the template in
Luddi96's config tool on lbre.de. If the two drift apart, the tool silently
produces a base64 string with the wrong layout and the device rejects it
(or worse, on older firmware, accepts it).

None of the changes in this fork touch confStruct, so this has to stay green.

  python3 test/test_confstruct.py
"""

import re
import sys
import urllib.request

TOOL_URL = "https://lbre.de/BREmote/struct.html"

FIELD = re.compile(r'^\s*((?:unsigned\s+)?[A-Za-z_][A-Za-z0-9_]*)\s+'
                   r'([A-Za-z_][A-Za-z0-9_]*(?:\[\d+\])?)\s*;')


def fields(text):
    """type/name pairs of the first struct confStruct block in text"""
    start = text.index('struct confStruct')
    depth, i = 0, text.index('{', start)
    for j in range(i, len(text)):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                body = text[i + 1:j]
                break
    else:
        raise ValueError('unterminated struct')

    body = re.sub(r'//.*', '', body)
    out = []
    for line in body.split('\n'):
        m = FIELD.match(line)
        if m:
            out.append((m.group(1), m.group(2)))
    return out


def template(html, key):
    blob = html[html.index('const templates'):html.index('function loadTemplate')]
    m = re.search(key + r"\s*:\s*`(.*?)`", blob, re.S)
    if not m:
        raise ValueError('template %s not found' % key)
    return m.group(1)


def main():
    try:
        html = urllib.request.urlopen(TOOL_URL, timeout=15).read().decode('utf-8')
    except Exception as e:
        print('skipped, could not reach %s (%s)' % (TOOL_URL, e))
        return 0

    failures = 0
    for dev, key in (('Rx', 'rx_v2'), ('Tx', 'tx_v2')):
        header = open('Source/V2_Integration_%s/BREmote_V2_%s.h' % (dev, dev),
                      encoding='utf-8').read()
        mine = fields(header)
        theirs = fields(template(html, key))

        if mine == theirs:
            print('%s confStruct matches %s (%d fields)' % (dev, key, len(mine)))
            continue

        failures += 1
        print('%s confStruct DIFFERS from %s' % (dev, key))
        for i in range(max(len(mine), len(theirs))):
            a = mine[i] if i < len(mine) else None
            b = theirs[i] if i < len(theirs) else None
            if a != b:
                print('  field %d: firmware %s, tool %s' % (i, a, b))

    print('\n%s' % ('TESTS FAILED' if failures else 'all tests passed'))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
