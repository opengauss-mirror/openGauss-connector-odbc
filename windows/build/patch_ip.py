import sys

file_path = sys.argv[1]
with open(file_path, 'r') as f:
    content = f.read()

header = '''#ifdef WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

'''

content = header + content

with open(file_path, 'w') as f:
    f.write(content)
