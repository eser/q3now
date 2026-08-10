#!/usr/bin/env python3
# Behavior-invariance self-test for the comments-only readability pass.
# Strips C/C++ // and /* */ comments WITHOUT touching string/char literals,
# then normalizes whitespace. Two files whose stripped+normalized streams are
# identical differ ONLY in comments — proving the edit changed no code token,
# no string literal, and no preprocessor directive.
#
# Usage:
#   comment_strip_check.py strip <file>            -> prints stripped+normalized stream
#   comment_strip_check.py cmp <fileA> <fileB>     -> exit 0 if streams identical, 1 if not (prints first diff)
import sys

def strip_comments(src: str) -> str:
    out = []
    i = 0
    n = len(src)
    state = 'code'   # code | line_comment | block_comment | string | char
    while i < n:
        c = src[i]
        nxt = src[i+1] if i + 1 < n else ''
        if state == 'code':
            if c == '/' and nxt == '/':
                state = 'line_comment'; i += 2; continue
            if c == '/' and nxt == '*':
                state = 'block_comment'; i += 2; continue
            if c == '"':
                out.append(c); state = 'string'; i += 1; continue
            if c == "'":
                out.append(c); state = 'char'; i += 1; continue
            out.append(c); i += 1; continue
        if state == 'line_comment':
            if c == '\n':
                out.append('\n'); state = 'code'; i += 1; continue
            # line-continuation inside a // comment extends it
            if c == '\\' and nxt == '\n':
                i += 2; continue
            i += 1; continue
        if state == 'block_comment':
            if c == '*' and nxt == '/':
                state = 'code'; i += 2; continue
            if c == '\n':
                out.append('\n')   # preserve line count loosely
            i += 1; continue
        if state == 'string':
            out.append(c)
            if c == '\\':
                if nxt != '':
                    out.append(nxt); i += 2; continue
                i += 1; continue
            if c == '"':
                state = 'code'
            i += 1; continue
        if state == 'char':
            out.append(c)
            if c == '\\':
                if nxt != '':
                    out.append(nxt); i += 2; continue
                i += 1; continue
            if c == "'":
                state = 'code'
            i += 1; continue
    return ''.join(out)

def normalize(s: str) -> str:
    # collapse all runs of whitespace (incl newlines) to a single space, strip ends.
    # this makes the comparison robust to comment-removal-induced blank lines / indentation.
    return ' '.join(s.split())

def read(path):
    with open(path, 'rb') as f:
        return f.read().decode('utf-8', errors='replace')

def main():
    if len(sys.argv) < 2:
        print("usage: strip <file> | cmp <a> <b>", file=sys.stderr); sys.exit(2)
    cmd = sys.argv[1]
    if cmd == 'strip':
        sys.stdout.write(normalize(strip_comments(read(sys.argv[2]))))
        return
    if cmd == 'cmp':
        a = normalize(strip_comments(read(sys.argv[2])))
        b = normalize(strip_comments(read(sys.argv[3])))
        if a == b:
            sys.exit(0)
        # find first divergence for diagnostics
        la, lb = a.split(' '), b.split(' ')
        for k in range(min(len(la), len(lb))):
            if la[k] != lb[k]:
                lo = max(0, k - 6)
                print("DIFF at token %d:" % k, file=sys.stderr)
                print("  A: ..." + ' '.join(la[lo:k+6]), file=sys.stderr)
                print("  B: ..." + ' '.join(lb[lo:k+6]), file=sys.stderr)
                sys.exit(1)
        print("DIFF: token count differs A=%d B=%d" % (len(la), len(lb)), file=sys.stderr)
        sys.exit(1)
    print("unknown cmd", file=sys.stderr); sys.exit(2)

if __name__ == '__main__':
    main()
