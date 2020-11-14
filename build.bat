pscp -r src pi@chesslr:workspace/chesslrcontroller
pscp README.md pi@chesslr:workspace/chesslrcontroller
pscp CMakeLists.txt pi@chesslr:workspace/chesslrcontroller

plink -batch pi@chesslr "cd workspace/chesslrcontroller && cmake -f CMakeLists.txt && make"

rem pscp pi@millipede:workspace/chesslrcontroller/chesslrcontroller .
rem pscp chesslrcontroller pi@chesslr:workspace/chesslrcontroller/chesslrcontroller