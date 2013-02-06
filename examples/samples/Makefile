all:
	$(CXX) $(CXXFLAGS) publisher.cpp  -o wave_publisher $(LDFLAGS) -lblackadder -lpthread
	$(CXX) $(CXXFLAGS) subscriber.cpp -o wave_subscriber $(LDFLAGS) -lblackadder -lpthread

clean:
	rm -f wave_publisher wave_subscriber 
