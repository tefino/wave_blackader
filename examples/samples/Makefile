all:
	$(CXX) $(CXXFLAGS) wave_publisher.cpp  -o publisher $(LDFLAGS) -lblackadder -lpthread
	$(CXX) $(CXXFLAGS) wave_subscriber.cpp -o subscriber $(LDFLAGS) -lblackadder -lpthread

clean:
	rm -f wave_publisher wave_subscriber 
