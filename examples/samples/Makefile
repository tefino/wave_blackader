all:
	$(CXX) $(CXXFLAGS) publisher.cpp  -o publisher $(LDFLAGS) -lblackadder -lpthread
	$(CXX) $(CXXFLAGS) subscriber.cpp -o subscriber $(LDFLAGS) -lblackadder -lpthread

clean:
	rm -f publisher subscriber 
