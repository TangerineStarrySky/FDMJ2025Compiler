RM       = rm -rf
MAKEFLAGS = --no-print-directory

.PHONY: build clean rebuild handin

BUILD_DIR = $(CURDIR)/build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release && make

clean:
	@$(RM) build ; \
	find test/fmj_normal -type f ! -name "*.fmj" -exec $(RM) {} +
	

rebuild: clean build

handin:
	@if [ ! -f docs/report.pdf ]; then \
		echo "请先在docs文件夹下攥写报告, 并转换为'report.pdf'"; \
		exit 1; \
	fi; \
	echo "请输入'学号-姓名' (例如: 12345678910-某个人)"; \
	read filename; \
	zip -q -r "docs/$$filename-final.zip" \
	  docs/report.pdf include lib

.PHONY: compile run compile-one run-one

MAIN = $(BUILD_DIR)/tools/main/main

compile: $(MAIN)
	cd $(CURDIR)/test/fmj_normal && \
	for file in $$(ls .); do \
		if [ "$${file#*.}" = "fmj" ]; then \
			echo "Reading $${file%%.*}.fmj"; \
			$(MAIN) "$${file%%.*}"; \
		fi; \
	done; \
	cd .. > /dev/null 2>&1 

run: $(MAIN)
	cd $(CURDIR)/test/fmj_normal && \
	for file in $$(ls .); do \
		if [ "$${file#*.}" = "6-xml.clr" ]; then \
			echo "------Reading $${file%%.*}------"; \
			arm-linux-gnueabihf-gcc -mcpu=cortex-a72 -Wall -Wextra --static -o "$${file%%.*}" "$${file%%.*}.s" ../../vendor/libsysy/libsysy32.s -lm; \
			# echo "Running the final assembly program........." ; \
			qemu-arm -B 0x1000 $${file%%.*}; \
			echo "Exit status: $$?"; \
		fi; \
	done; \
	cd .. > /dev/null 2>&1 

FILE=hw5test0

compile-one: $(MAIN)
	cd $(CURDIR)/test/fmj_normal && \
	echo "Reading ${FILE}.fmj"; \
	$(MAIN) "${FILE}"; \
	cd .. > /dev/null 2>&1 

run-one: $(MAIN)
	cd $(CURDIR)/test/fmj_normal && \
    arm-linux-gnueabihf-gcc -mcpu=cortex-a72 -Wall -Wextra --static -o "${FILE}" "${FILE}.s" ../../vendor/libsysy/libsysy32.s -lm; \
	# echo "Running the final assembly program........." ; \
	qemu-arm -B 0x1000 ${FILE}; \
	echo "Exit status of the command: $$?"; \
	cd .. > /dev/null 2>&1 
