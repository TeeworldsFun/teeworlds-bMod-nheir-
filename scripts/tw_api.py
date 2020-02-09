#!/usr/bin/env python3
# coding: utf-8
from socket import socket, AF_INET, SOCK_DGRAM
import sys
import threading
import time

import random

NUM_MASTERSERVERS = 4
MASTERSERVER_PORT = 8283


TIMEOUT = 2
BUFFER_SIZE = 4096
NUM_RETRIES =  3


# retries after waiting a specific time if the simple retry method failed.
# SLEEP_SECS is multiplied with 2 on every retry
FORCE_SLEEP = True
SLEEP_SECS = 2.0
NUM_SLEEP_RETRIES = 2

# src/mastersrv/mastersrv.h
PACKET_GETLIST = b"\xff\xff\xff\xffreq2"
PACKET_LIST = b"\xff\xff\xff\xfflis2"

PACKET_GETINFO = b"\xff\xff\xff\xffgie3"
PACKET_INFO = b"\xff\xff\xff\xffinf3"

# src/engine/shared/network.h
NET_PACKETFLAG_CONNLESS = 8
NET_PACKETVERSION = 1
NET_PACKETFLAG_CONTROL = 1
NET_CTRLMSG_TOKEN = 5
NET_TOKENREQUEST_DATASIZE = 512

def read_int_be(data, start=0, size=4):
	ret = 0
	for i in range(start, start+size):
		ret = (ret << 8) + data[i]
	return ret

# CVariableInt::Unpack from src/engine/shared/compression.cpp
def unpack_int(b):
	l = list(b[:5])
	i = 0
	Sign = (l[i]>>6)&1
	res = l[i] & 0x3F

	for _ in (0,):
		if not (l[i]&0x80):
			break
		i+=1
		res |= (l[i]&(0x7F))<<(6)

		if not (l[i]&0x80):
			break
		i+=1
		res |= (l[i]&(0x7F))<<(6+7)

		if not (l[i]&0x80):
			break
		i+=1
		res |= (l[i]&(0x7F))<<(6+7+7)

		if not (l[i]&0x80):
			break
		i+=1
		res |= (l[i]&(0x7F))<<(6+7+7+7)

	i += 1
	res ^= -Sign
	return res, b[i:]

class TwConn(threading.Thread):
	def __init__(self, address):
		threading.Thread.__init__(self, target = self.run)
		self.address = address
		self.local_token = None
		self.remote_token = None
		self.sock = None
		self.logs = []
		self.connless_handler = {}
		self.ctrlmsg_handler = {}
		self.finished = False
		self.retries = NUM_RETRIES

	def log(self, *msg):
		self.logs.append(msg)

	def initiate_conn(self):
		self.sock = socket(AF_INET, SOCK_DGRAM)
		self.sock.settimeout(TIMEOUT)

	def send_connless(self, msg):
		b = [0]*9
		# Header
		b[0] = ((NET_PACKETFLAG_CONNLESS<<2)&0xfc) | (NET_PACKETVERSION&0x03)
		b[1] = (self.remote_token >> 24) & 0xff
		b[2] = (self.remote_token >> 16) & 0xff
		b[3] = (self.remote_token >> 8) & 0xff
		b[4] = (self.remote_token) & 0xff
		# ResponseToken
		b[5] = (self.local_token >> 24) & 0xff
		b[6] = (self.local_token >> 16) & 0xff
		b[7] = (self.local_token >> 8) & 0xff
		b[8] = (self.local_token) & 0xff

		self.send(bytes(b) + msg)

	def send_ctrlmsg(self, token, ctrlmsg, msg):
		b = [0]*8
		b[0] = (NET_PACKETFLAG_CONTROL<<2)&0xfc
		# b[0:2] = ack/num_chunk
		b[3] = (token >> 24) & 0xff
		b[4] = (token >> 16) & 0xff
		b[5] = (token >> 8) & 0xff
		b[6] = (token) & 0xff
		b[7] = ctrlmsg

		self.send(bytes(b) + msg)

	def send_ctrlmsg_with_token(self, token, ctrlmsg, extended=False):
		b = [0]*NET_TOKENREQUEST_DATASIZE
		# Data
		b[0] = (self.local_token >> 24) & 0xff
		b[1] = (self.local_token >> 16) & 0xff
		b[2] = (self.local_token >> 8) & 0xff
		b[3] = (self.local_token) & 0xff

		if extended:
			b = bytes(b)
		else:
			b = bytes(b[:4])

		self.send_ctrlmsg(token, ctrlmsg, b)

	def send(self, data):
		self.log('send', data)
		return self.sock.sendto(data, self.address)

	def recv(self):
		data, addr = self.sock.recvfrom(BUFFER_SIZE)
		self.log('recv', data)

		flag = data[0] >> 2
		if flag & NET_PACKETFLAG_CONNLESS:
			version = data[0] & 3

			if version != NET_PACKETVERSION:
				self.log('wrong connless packet version, ignore')
				return

			token_recv = read_int_be(data, start=1)
			token_send = read_int_be(data, start=5)

			if self.remote_token != token_send or self.local_token != token_recv:
				self.log('wrong token in connless packet, using received ones')
				self.local_token = token_recv
				self.remote_token = token_send

			header = data[9:17]

			if header in self.connless_handler:
				self.connless_handler[header](data[17:])

		else:
			ack = read_int_be(data, size=2) & 0x3ff
			num_chunks = data[2]
			token_recv = read_int_be(data, start=3)

			if token_recv == 0xffffffff:
				token_recv = -1
			else:
				if token_recv != self.local_token:
					self.log("local token differs from received token, using the new token")
					self.local_token = token_recv

			if flag & NET_PACKETFLAG_CONTROL:
				control_msg = data[7]
				if control_msg == NET_CTRLMSG_TOKEN:
					self.remote_token = read_int_be(data, start=8)

				if control_msg in self.ctrlmsg_handler:
					self.ctrlmsg_handler[control_msg](data[8:])

	def request_token(self):
		if not self.local_token:
			self.local_token = random.randrange(0x100000000)

		self.send_ctrlmsg_with_token(-1, NET_CTRLMSG_TOKEN, True)

	def run(self):
		self.initiate_conn()

		while self.retries > 0:
			self.request_token()
			try:
				self.recv()
				break
			except OSError as e: # Timeout
				self.log(str(e))
			except Exception as e:
				# Shouldn't occurs
				import traceback
				traceback.print_exc()
			self.sock.settimeout(self.sock.gettimeout() * 2)
			self.retries -= 1

		self.sock.close()

		self.finished = True

def unpack_server_info(data):
	slots = data.split(b"\x00", maxsplit=5)

	server_info = {}
	server_info["version"] = slots[0].decode()
	server_info["name"] = slots[1].decode()
	server_info["hostname"] = slots[2].decode()
	server_info["map"] = slots[3].decode()
	server_info["gametype"] = slots[4].decode()
	data = slots[5]

	server_info["flags"], data = unpack_int(data)

	server_info["skill"], data = unpack_int(data)
	server_info["num_players"], data = unpack_int(data)
	server_info["max_players"], data = unpack_int(data)
	server_info["num_clients"], data = unpack_int(data)
	server_info["max_clients"], data = unpack_int(data)
	server_info["players"] = []

	for _ in range(server_info["num_clients"]):
		player = {}
		slots = data.split(b"\x00", maxsplit=2)
		player["name"] = slots[0].decode()
		player["clan"] = slots[1].decode()
		data = slots[2]
		player["country"], data = unpack_int(data)
		player["score"], data = unpack_int(data)
		player["player"], data = unpack_int(data)
		server_info["players"].append(player)

	return server_info

class Server_Info(TwConn):
	def __init__(self, address):
		TwConn.__init__(self, address)
		self.info = None
		self.connless_handler[PACKET_INFO] = self.server_info_handler
		self.ctrlmsg_handler[NET_CTRLMSG_TOKEN] = self.token_success

	def  __str__(self):
		return str(self.info)

	def __getitem__(self, key):
		return self.info[key]

	def token_success(self, *args):
		self.send_connless(PACKET_GETINFO + b'\x00')
		self.recv()

	def server_info_handler(self, data):
		browser_token, data = unpack_int(data)
		self.info = unpack_server_info(data)
		self.info['address'] = self.address

def unpack_server_list(data):
	servers = []
	num_servers = len(data) // 18

	for n in range(0, num_servers):
		# IPv4
		if data[n*18:n*18+12] == b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff":
			ip = ".".join(map(str, data[n*18+12:n*18+16]))
		# IPv6
		else:
			ip = ":".join(map(str, data[n*18:n*18+16]))
		port = ((data[n*18+16])<<8) + data[n*18+17]
		servers.append((ip, port))

	return servers

class Master_Server_Info(TwConn):
	def __init__(self, address):
		TwConn.__init__(self, address)
		self.servers = []
		self.connless_handler[PACKET_LIST] = self.server_list_handler
		self.ctrlmsg_handler[NET_CTRLMSG_TOKEN] = self.token_success

	def token_success(self, *args):
		self.send_connless(PACKET_GETLIST)
		self.recv()

	def server_list_handler(self, data):
		self.servers += unpack_server_list(data)

		try:
			self.recv()
		except OSError as e: # Timeout
			pass
		except Exception as e:
			# Shouldn't occurs
			import traceback
			traceback.print_exc()

if __name__ == '__main__':
	master_servers = []

	for i in range(1, NUM_MASTERSERVERS+1):
		m = Master_Server_Info(("master%d.teeworlds.com"%i, MASTERSERVER_PORT))
		master_servers.append(m)
		m.start()

	servers = set()

	while len(master_servers) != 0:
		master_servers[0].join()

		if master_servers[0].finished == True:
			if master_servers[0].servers:
				servers.update(master_servers[0].servers)
			del master_servers[0]


	servers_info = []

	print(len(servers), "servers")

	for server in servers:
		s = Server_Info(server)
		servers_info.append(s)
		s.start()

	num_players = 0
	num_clients = 0
	num_botplayers = 0
	num_botspectators = 0

	silent_servers = set()

	while len(servers_info) != 0:
		servers_info[0].join()

		if servers_info[0].finished == True:
			if servers_info[0].info:
				server_info = servers_info[0].info
				# check num/max validity
				if (server_info["num_players"] > server_info["max_players"]
					or server_info["num_clients"] > server_info["max_clients"]
					or server_info["max_players"] > server_info["max_clients"]
					or server_info["num_players"] < 0
					or server_info["num_clients"] < 0
					or server_info["max_clients"] < 0
					or server_info["max_players"] < 0
					or server_info["max_clients"] > 64):
					server_info["bad"] = 'invalid num/max'
					print('> Server %s has %s' % (server_info["address"], server_info["bad"]))
				# check actual purity
				elif server_info["gametype"] in ('DM', 'TDM', 'CTF', 'LMS', 'LTS') \
					and server_info["max_players"] > 16:
					server_info["bad"] = 'too many players for vanilla'
					print('> Server %s has %s' % (server_info["address"], server_info["bad"]))

				else:
					num_players += server_info["num_players"]
					num_clients += server_info["num_clients"]
					for p in servers_info[0].info["players"]:
						if p["player"] == 2:
							num_botplayers += 1
						if p["player"] == 3:
							num_botspectators += 1
			else:
				silent_servers.add(servers_info[0].address)
			del servers_info[0]

	print('%d/%d silent servers' % (len(silent_servers),len(servers)))
	print('%d players (%d bots) and %d spectators (%d bots)' % (num_players, num_botplayers, num_clients - num_players, num_botspectators))
