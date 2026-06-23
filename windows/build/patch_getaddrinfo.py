import sys

file_path = sys.argv[1]
with open(file_path, 'r') as f:
    content = f.read()

content = content.replace(
    'inet_net_ntop(AF_INET6, &((struct sockaddr_in6*)raddr)->sin6_addr, 128, node, nodelen)',
    'inet_net_ntop(AF_INET6, &((struct sockaddr_in6*)sa)->sin6_addr, 128, node, nodelen)'
)

content = content.replace(
    'ntohs(static_cast<struct sockaddr_in*>(sa)->sin_port)',
    'ntohs(((struct sockaddr_in*)sa)->sin_port)'
)

content = content.replace(
    'ntohs(static_cast<struct sockaddr_in6*>(sa)->sin6_port)',
    'ntohs(((struct sockaddr_in6*)sa)->sin6_port)'
)

with open(file_path, 'w') as f:
    f.write(content)
