docker build --squash -t inpxc:latest . -f .\Dockerfile.build | Tee-Object -FilePath "..\..\all.log"

# docker save -o ..\..\inpxc.tar inpxc
# docker load -i ..\..\inpxc.tar

# docker run --name inpxcreator -v E:/projects/books/InpxCreator:/root/result -it inpxc:latest
