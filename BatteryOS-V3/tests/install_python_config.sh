OS=`uname -s`
case "${OS}" in 
    Linux*)  
        echo sudo apt install python3-dev;;
    Darwin*) 
        echo brew install python;;
esac
