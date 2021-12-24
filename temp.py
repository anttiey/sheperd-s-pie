import socket

from gpiozero import MCP3008
from time import sleep

HOST = '192.168.0.38'
PORT = 8888

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((HOST, PORT))

lectura = MCP3008(channel=0)

while True:

    temperatura = (lectura.value * 3.3) * 100

    ftoint = '{}'.format(int(temperatura)) 

    print('Temperatura:', int(temperatura), 'C')
    client_socket.sendall(ftoint.encode())

    sleep(1)

client_socket.close()