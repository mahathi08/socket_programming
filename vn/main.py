import socket
import sys
import struct
import select
import time

def receive(sockfd,size):
    data = bytearray()
    while len(data) < size:
        b = sockfd.recv(size - len(data))
        if not b:
            raise ConnectionError("Connection closed")
        data.extend(b)
    return bytes(data)

def send_frame(sockfd,tmsg,body):
    sockfd.sendall(struct.pack("!cH",tmsg,len(body))+body)

def receive_frame(sockfd):
    h=receive(sockfd,3)
    tmsg,l=struct.unpack("!cH",h)
    return tmsg,receive(sockfd,l)

def encode_lsp(origin, seq, links):
    bod = bytearray(struct.pack("!ccIB",b"P",origin.encode(),seq,len(links)))
    for node, cost in sorted(links.items()):
        bod.extend(struct.pack("!c I", node.encode(), cost))
    return bytes(bod)

def decode_lsp(data):
    if len(data) < 7:
        raise ValueError("LSP is too short")
    tmsg, start, seq, cnt = struct.unpack("!ccIB", data[:7])
    if tmsg != b"P" or len(data) != 7 + cnt * 5:
        raise ValueError("Invalid LSP")
    origin = start.decode()
    if not ('A' <= origin <= 'Z'):
        raise ValueError("Invalid LSP origin")
    links = {}
    pos = 7
    for _ in range(cnt):
        node, cost = struct.unpack("!cI", data[pos:pos + 5])
        pos += 5
        node_name = node.decode()
        if not ('A' <= node_name <= 'Z'):
            raise ValueError("Invalid node in LSP")
        if node_name == origin:
            raise ValueError("LSP must not contain a self-link")
        if cost == 0 or node_name in links:
            raise ValueError("Invalid or duplicate link in LSP")
        links[node_name] = cost
    return origin, seq, links


def parse_ls(bod):
    if not bod:
        raise ValueError("empty link-state")
    k=bod[0]
    if len(bod)!=1+k*11:
        raise ValueError("invalid link-state length")
    neigh = []
    p = 1
    for i in range(k):
        nod=chr(bod[p])
        p+=1
        ip=socket.inet_ntoa(bod[p:p+4])
        p+=4
        port=struct.unpack("!H",bod[p:p+2])[0]
        p+=2
        cost=struct.unpack("!I",bod[p:p+4])[0]
        p+=4
        neigh.append((nod,ip,port,cost))
    self_entries = [entry for entry in neigh if entry[3] == 0]
    if len(self_entries) != 1:
            raise ValueError("should be one self-entry with cost zero")
    return self_entries[0][0],neigh


def print_graph(graph, vn_name):
    print(f"\nKnown graph at VN {vn_name}:")
    for start in sorted(graph):
        links = graph[start]
        print(f"{start}: {links}")


if len(sys.argv) != 3:
    print("Usage: python3 main.py <ON_IP> <VN_UDP_PORT>")
    sys.exit(1)

on_ip = sys.argv[1]
vn_udp_port = int(sys.argv[2])
# vn_ip = "172.23.32.143"

udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect((on_ip, 5000))
        vn_ip = probe.getsockname()[0]
    finally:
        probe.close()
    udp_sock.bind((vn_ip,vn_udp_port))

    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockfd.connect((on_ip,5000))
    print(f"VN on UDP port {vn_udp_port} connected to ON")
        
    conn = (socket.inet_aton(vn_ip)+struct.pack("!H",vn_udp_port))
    send_frame(sockfd,b"C",conn)
    tmsg, bod = receive_frame(sockfd)
    if tmsg != b"L":
        raise ValueError("Unexpected message type")
    vn_name,entries = parse_ls(bod)
    if not ('A' <= vn_name <= 'Z'):
        raise ValueError("Invalid VN identity")
    if len({entry[0] for entry in entries}) != len(entries):
        raise ValueError("Duplicate node in LINK-STATE")

    neighbours = {nod: (ip, port, cost) for nod, ip, port, cost in entries if nod != vn_name}

    own_links = {node: info[2] for node, info in neighbours.items()}
    seq = 1
    known_graph = {vn_name: own_links.copy()}
    seen_seq = {vn_name: seq}
    print(f"VN {vn_name}: initial neighbors = {own_links}")
    
    def send_ls():
        packet = encode_lsp(vn_name, seq, own_links)
        for node, (ip, port, cost) in neighbours.items():
            try:
                udp_sock.sendto(packet, (ip, port))
            except OSError as e:
                print(f"Could not send LSP to {node}: {e}")

    send_ls()
    last_sent = time.monotonic()
    last_print = last_sent
    
    while True:
        ready, _, _ = select.select([sockfd, udp_sock], [], [], 1)
        for ready_sock in ready:
            if ready_sock is sockfd:
                tmsg,bod=receive_frame(sockfd)
                if tmsg != b"L":
                    raise ValueError("Unexpected message type")
                new_name,entries = parse_ls(bod)
                if len({entry[0] for entry in entries}) != len(entries):
                    raise ValueError("Duplicate node in LINK-STATE")
                if new_name != vn_name:
                    raise ValueError("ON changed this VN's identity")
                new_neighbours = {nod: (ip, port, cost) for nod, ip, port, cost in entries if nod != vn_name}
            
                new_own_links = {node: info[2] for node, info in new_neighbours.items()}
                topology_changed = (new_neighbours != neighbours)
                neighbours = new_neighbours
                own_links = new_own_links
                known_graph[vn_name] = own_links.copy()
                if topology_changed:
                    if seq >= 0xFFFFFFFF:
                        raise OverflowError("LSP sequence number exhausted")
                    seq += 1
                    seen_seq[vn_name] = seq
                    send_ls()
                    last_sent = time.monotonic()
                    print(f"VN {vn_name}: topology updated")
            else:
                data, addr = udp_sock.recvfrom(65535)
                try:
                    start, recv_seq, links = decode_lsp(data)
                except (ValueError, struct.error):
                    print("Ignoring malformed LSP")
                    continue

                if start == vn_name:
                    continue
                if recv_seq <= seen_seq.get(start, 0):
                    continue
                seen_seq[start] = recv_seq
                known_graph[start] = links.copy()

                for node, (ip, port, cost) in neighbours.items():
                    if (ip, port) == addr:
                        continue
                    try:
                        udp_sock.sendto(data, (ip, port))
                    except OSError as e:
                        print(f"Could not forward LSP to {node}: {e}")

                print(f"VN {vn_name}: learned LSP from {start}, "f"sequence {recv_seq}")
            
        now = time.monotonic()
        if now - last_sent >= 15:
            if seq >= 0xFFFFFFFF:
                raise OverflowError("LSP sequence number exhausted")
            seq += 1
            seen_seq[vn_name] = seq
            known_graph[vn_name] = own_links.copy()
            send_ls()
            last_sent = now
            print(f"VN {vn_name}: periodic LSP sent")
        if now - last_print >= 15:
            print_graph(known_graph, vn_name)
            last_print = now

except KeyboardInterrupt:
    print("\nVN stopped by user")
except (OSError, ConnectionError, ValueError, OverflowError) as exc:
    print(f"VN error: {exc}", file=sys.stderr)
finally:    
    if "sockfd" in locals():
        sockfd.close()
    udp_sock.close()
