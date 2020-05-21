
CXXFLAGS := -g -O3

srcs := analysis.cc timers.cc utils.cc model.cc simulate.cc PMCMC.cc PART.cc poptree.cc
hdrs := $(wildcard *.h) $(wildcard *.hh)

exe := analysis

$(exe): $(srcs) $(hdrs)
	$(MPICXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(srcs) -o $(exe)

.PHONY : all
all: $(exe)

.PHONY : clean
clean:
	rm -f $(exe)
