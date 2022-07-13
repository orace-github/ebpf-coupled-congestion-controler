### Intro

The aim of this project is to be able to couple congestion controllers of several tcp connection using eBPF. 

### Build

#### Requirements

```
CONFIG_DEBUG_INFO_BTF=y
sudo apt update
sudo apt install libelf-dev
sudo apt-get install -y clang llvm  
```
#### Clone the repo and fetch lastest libbpf

```
git clone git@github.com:orace-github/ebpf-coupled-congestion-controler.git
cd ebpf-coupled-congestion-controler
git submodule init && git submodule update
```

#### Build eBPF code

To build the eBPF code execute the following commands.

```Bash
chmod +x build-bpf.sh
./build-bpf.sh
```

#### Bugs

If you have the following errors when building the eBPF code:

```
../.output/bpf/btf.h:426:24: error: use of undeclared identifier 'BTF_KIND_FLOAT'
        return btf_kind(t) == BTF_KIND_FLOAT;
                              ^
../.output/bpf/btf.h:431:24: error: use of undeclared identifier 'BTF_KIND_TAG'
        return btf_kind(t) == BTF_KIND_TAG;
                              ^

```

Just open the file bpf_cubic/btf in an editor and copy lines 23 and 24 to the file ../.output/bpf/btf.h. 
That will fix the error message.

#### Build libcc code

libcc couple cubic, reno, vegas controler congestion as an API

To build libcc code execute the following commands.

```Bash
chmod +x build-libcc.sh
./build-libcc.sh
```

#### Try with an exemple client and server

The libcc code expose to the application an API. Here are the functionnalities:

```C
enum bpfcc_t{
    BPF_CUBIC,
    BPF_RENO,
    BPF_VEGAS,
    BPF_BBR, // upcoming
    BPF_UNSPEC, // error type
    };

* This enum class list the controler congestion available

int bpfcc_select(enum bpfcc_t);
void bpfcc_unload();
void bpfcc_load();

* Then all application have to do is to select the controler congestion with enum bpfcc_t type, load and
unload respectively with bpfcc_load(), bpfcc_unload()
```

##### start the server

```
cd ../client_server
make clean && make
sudo ./server -s localhost -p 1234 -P 5678 -S send_log_file_path -R recv_log_file_path
```
The server calls load_bpf_cubic(void) to set bpf_cubic as default congestion controller. When the transfer ends
it calls  unload_bpf_cubic(void) to unload bpf_cubic.

##### start the client

The client does nothing because the test is done in the same host. Otherwise, the client will do the same.

In another terminal run the following command:

```
./client -s localhost -p 1234 -P 5678 -S ../send_log_file_path -R ../recv_log_file_path -i <src_file> -o <dst_file> -f
```

The client does nothing because the test is done in the same host. Otherwise, the client will do the same.

#### Ploting graph

```
python3 plot-bpf-cc-goodput.py client_log_simple.log client_log_simple.log  client_log_simple.log cubic vegas_bpf_cubic 100 60
```

cat /boot/config-5.13.0-051300-generic | grep CONFIG_DEBUG_INFO_BTF