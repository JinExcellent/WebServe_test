# 编译器设置
CXX = g++
TARGET = server

# 源文件列表（自动扫描子目录）
SRC_DIRS = . ./timer ./log ./mysql
SOURCES = $(wildcard *.cpp) $(wildcard timer/*.cpp) $(wildcard log/*.cpp) $(wildcard mysql/*.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SOURCES))

# 编译选项
DEBUG ?= 1
CXXFLAGS = -Wall -I. -I./mysql -I./log -I./timer  # 包含所有头文件路径
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2    # 注意是 -O2 不是 02
endif

# 链接选项（库放在最后）
LDFLAGS = -lpthread -lmysqlclient

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	find . -name "*.o" -delete
	rm -f $(TARGET)
