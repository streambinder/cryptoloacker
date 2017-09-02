# Introduction

This is the final project of _System programming_ university course. Professor's directions say it should be a server-client system for remote files ciphering (and/or deciphering), following a notorious malware path known as _ransomware_. Once installed to the attacked machine, the client will be the rudder for the attack. The application needs to support both _Linux_ and _Windows_ systems, operating exactly the same way on both.

## Name

Name choice has not been imposed by the professor, and descends from two fundamental reasons:

1. the paronomasia with _cryptolocker_, one of the most famous _ransomware_ attacks since 2013;

2. gives a playful idea if the project, developed for university-related cognitive purposes, marking the simplicity of the alphabetic encryption system used.

## Ramification

The project contains two main elements:

- `src` (folder): contains the source code, both for _Linux_ and _Windows_ platforms;

- `Makefile` (refular file): contains all the rules needed to automate the build processes.

# Structure

Physical files and folders structure has been defined that way it is to handle the distinction of every modulem, communication edge and host platform:

- communication edge:

  - `server`

  - `client`

- host platform:

  - _Linux_

  - _Windows_

Communication edge one gets done with `client` and `server` folders (immediately after `src` one); `common` folder is used to contain code used by both edges.

Any of this containers - `server` and/or `client` - contain the latter distinction, via `linux` and `windows` folders and, if needed, `common` one, too (similarly to the one already mentioned, it's used for _cross-platform_ code).

# Code

## _server_ module

### Specifications

As per professor's directions, `server` is made of several parts:

- a TCP socket needs to be instanciated to be listening on any network interface on conventional `8888` port (otherwise specified by `-p` input flag);

- the handling of any request gets delegated to a thread. The `n`-threads (conventrionally 4, or specified by the `-n` flag) are handled by a _threadpool_ (more details below);

- `server` operations are limited to the elements (recursively) contained into the folder mandatorily specified with `-c` flag.

- supported operations are the following:

  1. `LSTF`

      Lists the files contained in the folder, along with their bytes size;

  2. `LSTR`

      Recursively lists the files contained in the folder, along with their bytes size;

  3. `ENCR seed file`

      Ciphers, with a key generated using `seed`, the content of `file` into the `file_enc` and then removes `file`;

  4. `DECR seed file_enc`

      Deciphers, with a key generated using `seed`, the content of `file_enc` into the `file` and then removes `file_enc`;

- it's possible to specify a configuration files, with `-f` flag, within which to indicate the values corresponding to `-p` and`-n` flags.

It implementation follows some simple steps, explained below.

### Arguments parsing

First operation that gets done is the input arguments scan and their parsing. In order to do this, `getopt` library has been mainly used, because of its use simplicity in the flag-value association and in reporting facoltatives / mandatory input combinations. Later, parameters get validated, instead:

- the existence of configuration file: if existing, its values get parsed;

- the existence of the folder within which to relegate the application execution;

- specified port number validity;

- specified maximum threads number validity for _threadpool_.

### Instance and configuration of data structures and global variables

Once user input has been validated and handled, application effectively configures itself:

- _threadpool_ `init()` function gets called to initialize the maximum threads number (more details below);

- `WSADATA` structure get instanciated: it's actually used for socket handling procedures (only on _Windows_ platform);

- passive TCP socket gets instanciated on the specified port and fired to be listening for eventual connections.

### Connection management

Once an active socket is generated, its pointer gets passed as paramenter to the `handle_connection()` function, delegated of the server-client conversation management, for all its duration. The implementation of this method, within a `for` cycle, scans and reacts to every command requested by the other side of the communication:

- `LSTF`/`LSTR`: these two commands use the same ricorsive `list(char *ret_out, int recursive)` function (that respectively calls `list_opt(char *ret_out, int recursive, char *folder, char *folder_suffix)` function): while calling it, `LSTF` indirectly sets the boolean (an integer) `recursive` to 0, while `LSTR` does the same to 1. In both cases, `*folder` parameter will match with the `*arg_folder` pointer (the folder within which the application is in execution) and `*folder_suffix` will be NULLed. So, while scanning the `*folder` folder content, if another folder is met and `recursive` is _true_, then the function will call itself again, populating `*folder_suffix` variable adequately, to indicate the suffix that needs to be added to the initial forlder to construct the path of the just met folder.

- `ENCR`/`DECR`: exploiting the peculiarity of the ciphering made using the _exclusive disjunction_ (_XOR_) operator (given a _k_ key and ciphered a characters sequence applying the _XOR bitwise_ operation with _k_ key, reapplying the same operation with the same _k_, the the same initial sequence will be obtained), the implementation of the two commmands has been unified. In fact, they execute the same procedure (described more in detail below), except for the configuration of the input/output file.

### _threadpool_ module

_threadpool_ uses custom data structures to simplify both the complexity of the problems that it handles, and multi-platform. `job_t` is the most atomic structures, it contains all the informations about a task that needs to be executed within the _threadpool_: a function and its arguments pointers, and a pointer to the next `job_t`. Then, `threadpool_t` structure gathers useful informations for the correct functioning of the _threadpool_, such as the maximum threads number, or the threads list itself, or the _mutex_ and the _condition variable_, both used to regulate the interactions with internal fields of the structure itself.

There're several ways to interace with the _threadpool_ from the outside:

1. `threadpool_init()`

    This function initializes the data structures used by the module, and it's used to specify the maximum threads number usable in the _threadpool_ context.

2. `threadpool_add_job()`

    This function is delegated to handle the new operations - that will be marked as in pending - adding procedure to the _threadpool_ queue.

3. `threadpool_bye()`

    Finally, this function is invoked to make all the memory cleaning operations, before stopping the _threadpool_.

Every thread is configured to execute the module static function `thread_boot`, which do nothing but executing a task, or better, a `job_t`, actually in pending status on the _threadpool_ queue. This gets done once it has acquired the _lock_ on the _mutex_, so to update the informations about the next task and about the number of pending tasks.

### _cipher_ module

_cipher_ module follows a very simple structure, composed by only a `cipher()` function which gets multiple arguments: two `char` pointers - which correspond to the paths of the input/output files of the ciphering procedure - and an `unsigned int` used as seed for the key generation. The body of this function has three steps:

1. initialization of _file descriptor_ (s) (on _Linux_ platform) or `HANDLE` (s) (on _Windows_ platform) and of memory maps of the input/output files, once obtained the _lock_ on the first one;

2. effective ciphering of the first file to the latter;

3. closing of _file descriptor_ (s) (on _Linux_ platform) or `HANDLE` (on _Windows_ platform) and of the memory maps of the input/output files, once released the _lock_ on the first one.

#### Parallelization

Regarding the parallelization problem, has been allowed to use the _OpenMP_ _API_. This choice is motivated by two fundamental reasons:

1. it's a multi-platform _API_, so it won't need any code difference between the systems;

2. it's extremely simple to use.

As for the game rules imposed by the professor, the parallelization needs to be applied only if the file that will be ciphered is greater than 256 Kbyte. This is the reason why it has been chosen make the process work with two nested `for` cycles. The first one is parallelized using _OpenMP_, and will iterate on every 256 Kbyte long portion of the file. This way, if the file is less than 256 Kbyte, the `for` will include only a cycle, as it's executed sequentially. On the other side, the nested `for` cycle will iterate on every 4 byte, corresponding to an integer, that compose the 256 Kbyte. For any of these, ciphering will be calculated.

#### _one-time pad_ and parallelization problem

One of the encountered problems was about the needing of finding the simplest way to let the _one-time pad_ ciphering method and the parallelization on files greater than 256 Kbyte coexist. In fact, although the partitioning of the file into 256 Kbytes long blocks has simplified the parallelization problem, the parallelization itself has generated a new problem, because of its missing systematic of execution. In a sequential scenario it's provably true that for every iteration element, starting from the same seed, always the same key will be generated. In a parallelized scenario, this is not provable, as there's no way to foresee the `for` cycle execution order. In order to solve this problem, a new additional memory map is used to preventively and sequentially generate the ciphering keys. In fact, before executing the two nested `for` cycles for the effective ciphering procedure, a new memory map (of the same size of the input file) is instanciated and populated - with a new `for` cycle - with the keys generated using `rand_r()` (or `cipher_rand()`, on _Windows_ platform). On the old next `for` cycles, instead of dinamically generate the keys, every memory map key item corresponding to the iteration element will be used.

#### `cipher_rand()` implementation on _Windows_ platform

As easily deductible reading _Windows_ module's code variant, there's a function which is not present on the same module's _Linux_ implementation: `cipher_rand()`. It consists of a pseudo-random number generator, used to generate the key from the seed. The reason behind this choice is about the lack of a system implementation of such a function on _Windows_ platform. So, in this case, the method - taken from the implementation of `rand_r()` offered by _MinGW_ (more details below) - has been provided.

```c
static int cipher_rand(unsigned int *seed) {
        long k;
        long s = (long)(*seed);
        if (s == 0) {
                s = 0x12345987;
        }
        k = s / 127773;
        s = 16807 * (s - k * 127773) - 2836 * k;
        if (s < 0) {
                s += 2147483647;
        }
        (*seed) = (unsigned int)s;
        return (int)(s & RAND_MAX);
}
```

## _client_ module

`client` is the simplest code portion of the project. It's made of three parts:

1. arguments parsing

    Unlike the `server` implementation, in this case no external auxiliary library has been used to handle the arguments parsing problem; instead, a more artisanal method that could fit around the case needs has been preferred. In fact, the ambiguity between the input flags that don't need arguments and the ones that actually do, between flags that indicate analogues commands, or that - `server` side - need arguments, has brought to this choice.

2. creation of socket, needed to connect to a `server`

    Regarding the socket management, it's the most reduntant code portion, if compared to the one from `server` module; they only have a difference: the `server` socket is waiting for connections, the `client` one is requesting a connection to a previously `server` allocated one, instead.

3. _back-and-forth_ with server

    In this phase, what happens will follow this simple scheme:

    - translation of the `client` input flags into `server` supported commands (_e.g._, `client` flag `-l` gets translated into `server` command `LSTF`);

    - sending command to `server` via socket;

    - receiving a reply from `server` and printing the reply itself.

# How to use

## Compilation

### Linux

The configuration of the development environment and the subsequent compilation on _Linux_ platform is relatively simple, as most of _Linux_ distribution provides a base packages group for the development. In the case of the environment where the code has been written:

`# eopkg it -c system.devel`

Now, just a `make` is enough:

`# make [server|client]`

#### Tested on

This software has been written and tested on the following environment:

|        Linux | 4.9.45 x86_64               |
| -----------: | :-------------------------- |
| Distribution | Solus Project               |
|          RAM | 20 Gb                       |
|          CPU | Intel Core i7-4770k Haswell |
|         Type | Phisical machine            |

### Windows

Configuration and compilation of project on a _Windows_ environment is a little bit more complicated. Auxiliary libraries have been used to simplify the compilation phase, and reduce the differentiation of compilation template listed in the `Makefile` to the minimum: it's the reason why `msys2` and `mingw-w64` have been adopted. The configuration proceeds this way:

1. `msys2` installation via official site: <http://www.msys2.org>

2. Application launching and subsequent update of packages database: `# pacman -Syu`

3. Effective packages update: `# pacman -Su`

4. `minGW-w64` installation: `# pacman -S mingw-w64-x86_64-gcc` (on 32 bit architecture, install `mingw-w64-i686-gcc` instead)

5. Base development dependencies installation: `# pacman -S base-devel`

6. _optional:_ in order to use the packages installed above even from the PowerShell, adding the path of the binary files to the global _Windows_ `PATH` variable is required: `C:\path\to\msys2\usr\bin` e `C:\path\to\msys2\mingw64\bin`.

#### Tested on

This software has been written and tested on the following environment:

| Windows | 10 Pro x86_64               |
| ------: | :-------------------------- |
|     RAM | 4096 Mb                     |
|     CPU | Intel Core i7-4770k Haswell |
|    Type | Virtual machine             |

## How to use

### Server

`server` can be used the following ways (assuming `pwd` is the root folder of the project, once it has been compiled):

`# ./bin/cryptoloackerd -c /path [-n max-threads -p port]`

You can specify the `/path` and the `port` nnumber in a configuration file and let the `server` load those values reading the configuration file itself, specifying it as parameter:

`# ./bin/cryptoloackerd -f /path/file.txt [-n max-threads]`

In this case, file will be populated following this template:

```
# cat /path/file.txt
folder = /path
port = 8888
```

### Client

`client` can be used the following ways (assuming `pwd` is the root folder of the project, once it has been compiled):

- To execute `LSTF` or `LSTR`:

  `# ./bin/cryptoloacker -h server-ip -p port [-l|-R]`

- To execute `ENCR` or `DECR`:

  `# ./bin/cryptoloacker -h server-ip -p port [-e|-d] seed /path/file`
