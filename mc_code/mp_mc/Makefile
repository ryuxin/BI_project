all: 
	make -C clients
	make -C servers
	cp ./clients/client .
	cp ./servers/server .
	dd if=/dev/zero of=mapping_file bs=1M count=1

clean:
	rm -f ./client
	rm -f ./server
	rm -f ./mapping_file
	make -C clients clean
	make -C servers clean
