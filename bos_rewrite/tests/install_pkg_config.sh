OS=`uname -s`
case "${OS}" in 
    Linux*)  
        sudo apt install -y pkg-config;;
    Darwin*) 
        brew install pkg-config;;
esac
