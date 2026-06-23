import sys

file_path = sys.argv[1]
with open(file_path, 'r') as f:
    content = f.read()

content = content.replace(
    '#include <stdlib.h>\n#include "utils/syscall_lock.h"',
    '#include <stdlib.h>\n#include <stdio.h>\n#include <string.h>\n#include "utils/syscall_lock.h"'
)

setenv_func = '''static int setenv(const char* name, const char* envvar, int overwrite)
{
    char* buf = NULL;
    if (!overwrite && getenv(name) != NULL)
        return 0;
    buf = (char*)malloc(strlen(name) + strlen(envvar) + 2);
    if (buf == NULL)
        return -1;
    (void)sprintf_s(buf, strlen(name) + strlen(envvar) + 2, "%s=%s", name, envvar);
    return _putenv(buf);
}
'''

content = content.replace(
    '#define unsetenv(x) pgwin32_unsetenv(x)\n\nint gs_putenv_r(char* envvar);',
    '#define unsetenv(x) pgwin32_unsetenv(x)\n\n' + setenv_func + '\nint gs_putenv_r(char* envvar);'
)

with open(file_path, 'w') as f:
    f.write(content)
