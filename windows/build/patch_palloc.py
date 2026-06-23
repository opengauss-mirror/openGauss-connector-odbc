import sys

file_path = sys.argv[1]
with open(file_path, 'r') as f:
    content = f.read()

content = content.replace(
    '#ifndef FRONTEND_PARSER\n#include <iostream>\n#include <memory>',
    '#ifndef FRONTEND_PARSER\n#include <errno.h>\n#ifndef EAGAIN\n#define EAGAIN 11\n#endif\n#ifdef read\n#undef read\n#endif\n#ifdef write\n#undef write\n#endif\n#include <iostream>\n#include <memory>'
)

with open(file_path, 'w') as f:
    f.write(content)
