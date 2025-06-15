RM       = rm -rf
MAKEFLAGS = --no-print-directory

.PHONY: build clean rebuild handin

BUILD_DIR = $(CURDIR)/build

build:
	@cmake -G Ninja -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release; \
	cd $(BUILD_DIR) && ninja

clean: 
	@$(RM) build ; \
	$(RM) test/*5.* \

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

.PHONY: run run-one gentests gentest-one patchdemo 

MAIN = $(BUILD_DIR)/tools/main/main

run-assem: $(MAIN)
	cd $(CURDIR)/test && \
	for file in $$(ls .); do \
		if [ "$${file#*.}" = "6-xml.clr" ]; then \
			echo "------Reading $${file%%.*}------"; \
			arm-linux-gnueabihf-gcc -mcpu=cortex-a72 -Wall -Wextra --static -o "$${file%%.*}" "$${file%%.*}.s" ../vendor/libsysy/libsysy32.s -lm; \
			echo "Running the final assembly program........." ; \
			qemu-arm -B 0x1000 $${file%%.*}; \
		fi; \
	done; \
	cd .. > /dev/null 2>&1 

FILE=bubblesort

run-one-assem: $(MAIN)
	cd $(CURDIR)/test && \
    arm-linux-gnueabihf-gcc -mcpu=cortex-a72 -Wall -Wextra --static -o "${FILE}" "${FILE}.s" ../vendor/libsysy/libsysy32.s -lm; \
	echo "Running the final assembly program........." ; \
	qemu-arm -B 0x1000 ${FILE}; \
	cd .. > /dev/null 2>&1 
