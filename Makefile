default: ${SOURCE_FILE}
	gcc simp2p.c -lpthread -o simp2p
clean:
	rm simp2p
run:
	./simp2p
