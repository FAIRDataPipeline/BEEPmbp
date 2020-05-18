
CXXFLAGS := -g -O3

srcs := analysis.cc var.cc functions.cc model.cc simulate.cc PMCMC.cc PART.cc init.cc
hdrs := PART.hh PMCMC.hh consts.hh functions.hh gitversion.hh init.hh model.hh simulate.hh types.hh var.hh

exe := analysis

$(exe): $(srcs) $(hdrs) | gitversion
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(srcs) -o $(exe)

.PHONY : gitversion
gitversion:
	./gitversion.sh gitversion.hh

.PHONY : all
all: $(exe)

.PHONY : clean
clean:
	rm -f $(exe)
