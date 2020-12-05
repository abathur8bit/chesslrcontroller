pscp -r -p src pi@millipede:workspace/chesslrcontroller
pscp -p README.md pi@millipede:workspace/chesslrcontroller
pscp -p CMakeLists.txt pi@millipede:workspace/chesslrcontroller

plink -batch pi@millipede "cd workspace/chesslrcontroller && cmake -DCMAKE_BUILD_TYPE=Debug -f CMakeLists.txt && make"
plink -batch pi@chesslr "kill `ps aux | grep chesslrcontroller | grep -v grep | awk -F' ' '{print $2}'`"

pscp pi@millipede:workspace/chesslrcontroller/chesslrcontroller .
pscp chesslrcontroller pi@chesslr:workspace/chesslrcontroller
plink -batch pi@chesslr "chmod 755 workspace/chesslrcontroller"
