TARGET = correlation

$(TARGET): Correlation.o $(LIB)
	g++ $^ -o $(TARGET) -O3

Correlation.o: Correlation.cpp
	g++ -c $^ -o $@ -std=c++14 

clean:
	rm -f *.o $(TARGET)

