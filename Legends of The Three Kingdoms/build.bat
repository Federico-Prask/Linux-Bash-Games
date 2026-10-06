@echo off
g++ -std=c++17 main.cpp src/Platform.cpp src/Card.cpp src/Player.cpp src/Hero.cpp src/Skills.cpp src/SkillsStd.cpp src/SkillsMyth.cpp src/SkillsMou.cpp src/SkillsExtra.cpp src/SkillsNewHeroes.cpp src/SkillsMode.cpp src/HeroRegistry.cpp src/HeroTier.cpp src/Equipment.cpp src/GameEngine.cpp src/AI.cpp src/Logger.cpp src/Interaction.cpp src/Roles.cpp src/Codex.cpp src/CardTracker.cpp -Iinclude -o thks.exe
if errorlevel 1 exit /b %errorlevel%
echo Build succeeded: thks.exe
