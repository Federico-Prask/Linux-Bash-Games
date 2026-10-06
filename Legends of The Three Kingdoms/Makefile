CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
TARGET = thks
SRCS = main.cpp src/Platform.cpp src/Card.cpp src/Player.cpp src/Hero.cpp src/Skills.cpp src/SkillsStd.cpp src/SkillsMyth.cpp src/HeroRegistry.cpp src/HeroTier.cpp src/Equipment.cpp src/GameEngine.cpp src/AI.cpp src/Logger.cpp src/SkillsMou.cpp src/SkillsExtra.cpp src/SkillsNewHeroes.cpp src/SkillsMode.cpp src/CardTracker.cpp src/Interaction.cpp src/Roles.cpp src/Codex.cpp
OBJS = $(SRCS:.cpp=.o)

# 单元测试（不链接 main.cpp）
TESTSRCS = $(filter-out main.cpp,$(SRCS))
TESTTARGET = tests/run_tests

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(TESTTARGET)
	./$(TESTTARGET)

$(TESTTARGET): $(wildcard tests/*.cpp) $(TESTSRCS)
	$(CXX) $(CXXFLAGS) -Itests tests/*.cpp $(TESTSRCS) -o $@

clean:
	rm -f src/*.o *.o $(TARGET) $(TARGET).exe $(TESTTARGET)

.PHONY: all test clean

-include $(OBJS:.o=.d)
