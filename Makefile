buildDir ?= build
releaseType ?= Release

.PHONY: all
all: ${buildDir}/scrappie

${buildDir}:
	mkdir ${buildDir}

.PHONY: test
test: ${buildDir}/scrappie
	cd ${buildDir} && \
	make test

.PHONY: clean
clean:
	rm -rf ${buildDir}

${buildDir}/scrappie: ${buildDir}
	cd ${buildDir} && \
	cmake .. -DCMAKE_BUILD_TYPE=${releaseType} && \
	make
    
