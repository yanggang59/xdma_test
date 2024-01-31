TEST_DIR:=test
SRC_DIR:=src

all :
	@/bin/mkdir -p build
	$(MAKE) -C $(SRC_DIR)
	$(MAKE) -C $(TEST_DIR)

clean:
	$(MAKE) -C $(TEST_DIR) clean
	$(MAKE) -C $(SRC_DIR) clean
	@/bin/rm -rf build
