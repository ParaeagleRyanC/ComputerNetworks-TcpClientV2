# TCP Client v2

TCP Client v2 is built with [TCP Client V1](https://github.com/ParaeagleRyanC/ComputerNetworks-TcpClientV1) as its base version. It uses the same protocol, but with minor adjustments.

This program adds `pipelining` to the protocol. In this context, pipelining is sending multiple requests in one TCP connection. In **v1**, if you wanted to send multiple requests, you had to start a TCP connect for each request. This can be time consuming because of TCP's three-way handshake. Rather than starting a new TCP connection for each request, the TCP socket can be reused for sending multiple requests.

After make-ing the program, in a terminal, a request can be sent in the following format: `ACTION LENGTH MESSAGE`.

The ACTION can be one of the following 5 options:

* Uppercase
* Lowercase
* Reverse
* Shuffle
* Random

The response will be in the format of `LENGTH TRANSFORMED_MESSAGE`.
Where `LENGTH` is the length of the `TRANSFORMED_MESSAGE`.

For example, if you, as the client, send two pipelined commands,
```
reverse 11 Hello World
uppercase 11 Hello World
```
The response from the server would be 
```
11 dlroW olleH11 HELLO WORLD
```
