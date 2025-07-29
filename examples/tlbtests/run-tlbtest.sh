echo 'testing tlbtest for 8 pages'
./tlbtest-runner tlbtest-8 eyrie-rt loader.bin
echo 'testing tlbtest for 16 pages'
./tlbtest-runner tlbtest-16 eyrie-rt loader.bin
echo 'testing tlbtest for 32 pages'
./tlbtest-runner tlbtest-32 eyrie-rt loader.bin
echo 'testing tlbtest for 64 pages'
./tlbtest-runner tlbtest-64 eyrie-rt loader.bin
echo 'testing tlbtest for 128 pages'
./tlbtest-runner tlbtest-128 eyrie-rt loader.bin
echo 'testing tlbtest for 256 pages'
./tlbtest-runner tlbtest-256 eyrie-rt loader.bin
echo 'testing tlbtest for 512 pages'
./tlbtest-runner tlbtest-512 eyrie-rt loader.bin
echo 'testing tlbtest for 1024 pages'
./tlbtest-runner tlbtest-1024 eyrie-rt loader.bin