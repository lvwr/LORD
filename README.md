# LORD
LORD is a QEMU / DBT based Program Shepherding/Shadow Stack implementation for
x86 processes. LORD is capable of detecting anomalous program-flows, such as it
happens afeter control-flow hijacking attacks.

## How does it work:

During x86 translation, QEMU instruments the generated source code with snippets
that output traces to another process running in parallel, which we call LORD.

LORD implements different policies for verifying if the trace is correct or if
there may have been an issue.

## How do I compile the modified qemu:

First you have to pick a lord communication protocol version

SHEPH\_NOPT\_WRITE:
Uses pipe for ipc, has no local buffer - 1 call/ret generates 1 ipc msg

SHEPH\_BUFFER:
Uses pipe for ipc, optimized with a local buffer to store consecutive calls
Genrates ipc msgs with full buffer content on rets

SHEPH\_STACK\_WRITE:
Same as SHEPH\_BUFFER + WRITE debugging messages

SHEPH\_STACK:
Same as SHEPH\_BUFFER
Optimized with a local stack to assert consecutive calls/rets

SHEPH\_SHARED:
Uses shared memory for ipc

SHEPH\_LEAF:
No program shepherding / shadow stack here.
Just an experiment for counting leaf functions.

For more information, see the [paper]
(http://ieeexplore.ieee.org/document/6970646/?reload=true&arnumber=6970646)

After choosing one of the above, edit the file Makefile.target in LORD/qemu and
make sure you include one of the agove options to QEMU\_CFLAGS. As in:

QEMU\_CFLAGS+= -DSHEPH\_SHARED

Finally,

./configure --target-list=x86\_64-linux-user
make

## How do I compile lord:

cd LORD/lord

Each file in this folder is a different version of the verifier. Pick one. The
only version compatible with shared memory is lord-shared64.c. For more
information on the different versions, see the paper.

Compile the desired verifier:
gcc lord-shared64.c -o shared
g++ lord-stack64.cpp -o stack

## How do I run lord?

cd LORD/qemu/x86\_64-linux-user/
./qemu-x86\_64 -sh <lord-binary> <binary to be verified>

## License

While QEMU holds its own licensing, all other source files in LORD are relased
under GNU GPLv3 License. Licensing info can be found at LORD/lord/LICENSE

## What else?

Not much else, enjoy :-)

## Authors:
João Moreira - joao.moreira@lsc.ic.unicamp.br

Lucas Teixeira -

Sandro Rigo - sandro@ic.unicamp.br
