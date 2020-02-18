#!/usr/bin/env python3
# python3 -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. csci4220_hw4.proto

from concurrent import futures
import sys  # For sys.argv, sys.exit()
import socket  # for gethostbyname()
import math

import grpc

import csci4220_hw4_pb2
import csci4220_hw4_pb2_grpc

this_node = None
n = 4

class KadImplServicer(csci4220_hw4_pb2_grpc.KadImplServicer):

	this_node = None
	k_buckets = []
	k = 0
	values = {}

	# FINDNODE rpc
	def FindNode(self, request, context):
		print("Serving FindNode("+str(request.idkey)+") request for "+str(request.node.id))

		# Determine k closest nodes
		temp = []
		for i in range(len(self.k_buckets)):
			for j in range(len(self.k_buckets[i])):
				temp.append(self.k_buckets[i][j])
		if request.node in temp:
			temp.remove(request.node)

		while len(temp) > self.k:
			longest_node = None
			longest_val = 0
			for node in temp:
				distance = node.id ^ request.idkey
				if distance > longest_val:
					longest_val = distance
					longest_node = node
			temp.remove(longest_node)

		nodelist = csci4220_hw4_pb2.NodeList(
			responding_node = self.this_node,
			nodes = temp
		)

		# Adding requesting node to k_buckets
		k_buckets_add(self, request.node)

		return nodelist

	# FINDVALUE rpc
	def FindValue(self, request, context):
		print('Serving FindKey('+str(request.idkey)+') request for '+str(request.node.id))

		# Check if key is stored locally
		if request.idkey in self.values:
			return csci4220_hw4_pb2.KV_Node_Wrapper(
				responding_node = self.this_node,
				mode_kv = True,
				kv = csci4220_hw4_pb2.KeyValue(
					node = self.this_node,
					key = request.idkey,
					value = self.values[request.idkey]
				)
			)
		else:
			# Determine k closest nodes to key
			temp = []
			for i in range(len(self.k_buckets)):
				for j in range(len(self.k_buckets[i])):
					temp.append(self.k_buckets[i][j])
			if request.node in temp:
				temp.remove(request.node)

			while len(temp) > self.k:
				longest_node = None
				longest_val = 0
				for node in temp:
					distance = node.id ^ request.idkey
					if distance > longest_val:
						longest_val = distance
						longest_node = node
				temp.remove(longest_node)

			return csci4220_hw4_pb2.KV_Node_Wrapper(
				responding_node = self.this_node,
				mode_kv = False,
				nodes = temp
			)


	# STORE rpc
	def Store(self, request, context):
		print('Storing key '+str(request.key)+' value "'+request.value+'"')

		self.values[request.key] = request.value

		return csci4220_hw4_pb2.IDKey(
			node = this_node,
			idkey = request.key
		)

	# QUIT rpc
	def Quit(self, request, context):
		# Find node in bucket
		for i in range(len(self.k_buckets)):
			if request.node in self.k_buckets[i]:

				print("Evicting quitting node "+str(request.node.id)+" from bucket "+str(i))

				self.k_buckets[i].remove(request.node)

				return csci4220_hw4_pb2.IDKey(
					node = self.this_node,
					idkey = request.node.id
				)

		return None


# Prints k_buckets
def print_k_buckets(k_buckets):
	for i in range(n):
		print(i, end=":")
		for node in reversed(k_buckets[i]):
			print(" " + str(node.id) + ":" + str(node.port), end="")
		print()

# Adds node to k_buckets
def k_buckets_add(servicer, node):
	if node.id != servicer.this_node.id:
		bucket = math.floor(math.log(node.id ^ servicer.this_node.id, 2))
		if node not in servicer.k_buckets[bucket]:
			servicer.k_buckets[bucket].insert(0,node)
			servicer.k_buckets[bucket] = servicer.k_buckets[bucket][:servicer.k]
		else:
			servicer.k_buckets[bucket].remove(node)
			servicer.k_buckets[bucket].insert(0,node)
			servicer.k_buckets[bucket] = servicer.k_buckets[bucket][:servicer.k]

# Returns true if node is contained in k_buckets
def has_node(servicer, node):
	bucket = math.floor(math.log(node.id ^ servicer.this_node.id, 2))
	return node in servicer.k_buckets[bucket]

def run():
	if len(sys.argv) != 4:
		print("Error, correct usage is {} [my id] [my port] [k]".format(sys.argv[0]))
		sys.exit(-1)

	local_id = int(sys.argv[1])
	my_port = str(int(sys.argv[2])) # add_insecure_port() will want a string
	k = int(sys.argv[3])
	my_hostname = socket.gethostname() # Gets my host name
	my_address = socket.gethostbyname(my_hostname) # Gets my IP address from my hostname


	

	# Create server
	server = grpc.server(futures.ThreadPoolExecutor(max_workers=16))
	servicer = KadImplServicer()
	csci4220_hw4_pb2_grpc.add_KadImplServicer_to_server(servicer, server)
	server.add_insecure_port('[::]:'+my_port)
	server.start()

	servicer.k = k
	for i in range(n):
		servicer.k_buckets.append([])


	servicer.this_node = csci4220_hw4_pb2.Node(
		id = local_id,
		port = int(my_port),
		address = my_address
	)


	# Listen for commands from standard input
	while 1:
		for line in sys.stdin:
			command = line.split(" ")
			for i in range(len(command)):
				command[i] = command[i].strip()

			# BOOTSTRAP command
			if (command[0] == "BOOTSTRAP"):
				remote_addr = socket.gethostbyname(command[1])
				remote_port = int(command[2])
				with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
					stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)

					nodeList = stub.FindNode(csci4220_hw4_pb2.IDKey(
						node = servicer.this_node,
						idkey = servicer.this_node.id
					))

					k_buckets_add(servicer, nodeList.responding_node)

					# Add nodes in nodeList to k_buckets
					for node in nodeList.nodes:
						k_buckets_add(servicer, node)

					print("After BOOTSTRAP("+str(nodeList.responding_node.id)+"), k_buckets now look like:")
					print_k_buckets(servicer.k_buckets)


			# FIND_NODE command
			if (command[0] == "FIND_NODE"):
				print("Before FIND_NODE command, k-buckets are:")
				print_k_buckets(servicer.k_buckets)

				key = int(command[1])
				found = 0

				if local_id == key:
					print("Found destination id "+str(key))
				else:
					for i in range(n):
						for j in range(len(servicer.k_buckets[i])):
							if found == 0:
								with grpc.insecure_channel(servicer.k_buckets[i][j].address + ':' + str(servicer.k_buckets[i][j].port)) as channel:
									stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)

									nodelist = stub.FindNode(csci4220_hw4_pb2.IDKey(
										node = servicer.this_node,
										idkey = key
									))

									for node in nodelist.nodes:
										k_buckets_add(servicer, node)
										if node.id == key:
											print("Found destination id "+str(key))
											found = 1
										

				if found == 0:
					print("Could not find destination id "+str(key))


				print("After FIND_NODE command, k-buckets are:")
				print_k_buckets(servicer.k_buckets)


			# FIND_VALUE command
			if (command[0] == "FIND_VALUE"):
				print("Before FIND_VALUE command, k-buckets are:")
				print_k_buckets(servicer.k_buckets)

				key = int(command[1])
				found = 0

				# First check if key is stored locally
				if key in servicer.values:
					print('Found data "'+servicer.values[key]+'" for key '+str(key))
					found = 1
				else:
					# Find node with id closest to key
					closest = None
					for i in range(n):
						for j in range(len(servicer.k_buckets[i])):
							if closest == None:
								closest = servicer.k_buckets[i][j]
							if (servicer.k_buckets[i][j].id ^ key) < (closest.id ^ key):
								closest = servicer.k_buckets[i][j]

					# Check if bucket is empty
					if closest != None:
						# Ask closest node for value
						with grpc.insecure_channel(closest.address + ':' + str(closest.port)) as channel:
							stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)

							kv_wrapper = stub.FindValue(csci4220_hw4_pb2.IDKey(
								node = servicer.this_node,
								idkey = key
							))

							k_buckets_add(servicer, kv_wrapper.responding_node)

							# If value was found for key
							if kv_wrapper.mode_kv:
								print('Found value "'+kv_wrapper.kv.value+'" for key '+str(key))
								found = 1
							else:
								for node in kv_wrapper.nodes:
									k_buckets_add(servicer, node)
									# Correct node found, ask for value
									if node.id == key:
										with grpc.insecure_channel(node.address + ':' + str(node.port)) as channel:
											stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)

											kv_wrapper1 = stub.FindValue(csci4220_hw4_pb2.IDKey(
												node = servicer.this_node,
												idkey = key
											))

											if kv_wrapper1.mode_kv:
												print('Found value "'+kv_wrapper1.kv.value+'" for key '+str(key))
												found = 1

								

				if found == 0:
					print("Could not find key "+str(key))

				print("After FIND_VALUE command, k-buckets are:")
				print_k_buckets(servicer.k_buckets)

			# STORE command
			if (command[0] == "STORE"):
				key = int(command[1])
				value = command[2]

				# Find node with id closest to key
				closest = servicer.this_node
				for i in range(n):
					for j in range(len(servicer.k_buckets[i])):
						if (servicer.k_buckets[i][j].id ^ key) < (closest.id ^ key):
							closest = servicer.k_buckets[i][j]

				# Check if this is the closest node -> store locally
				if closest.id == servicer.this_node.id:
					servicer.values[key] = value
				else:
					# Send value to closest node
					with grpc.insecure_channel(closest.address + ':' + str(closest.port)) as channel:
						stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)

						stub.Store(csci4220_hw4_pb2.KeyValue(
							node = servicer.this_node,
							key = key,
							value = value
						))

				print("Storing key "+str(key)+" at node "+str(closest.id))

			# QUIT command
			if (command[0] == "QUIT"):
				for i in reversed(range(n)):
					for node in reversed(servicer.k_buckets[i]):
						with grpc.insecure_channel(node.address + ':' + str(node.port)) as channel:
							stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)

							print("Letting "+str(node.id)+" know I'm quitting.")
							stub.Quit(csci4220_hw4_pb2.IDKey(
								node = servicer.this_node,
								idkey = servicer.this_node.id
							))

				print("Shut down node "+str(local_id))
				sys.exit()


if __name__ == '__main__':
	run()