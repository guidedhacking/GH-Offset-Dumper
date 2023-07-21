# Guided Hacking Offset Dumper aka GH Offset Dumper  

Version 0.7

### What does it do
Externally scan a process for signatures and dump the relative offsets to a header file which is easy to incorporate into your Visual Studio project.  When an update is released for a game, you run the dumper to get the latest offsets.

### Why
Scrubs don't know how to pattern scan so they manually update their offsets in their game hacks after running an offset dumper like this.

### How to use
1. Put an updated config.json in the same folder as GH-Offset-Dumper.exe
2. Run the game
3. Run the dumper
4. Include the .h file which gets generated into your project


### How is this different from HazeDumper?
This dumper was inspired by [hazedumper](https://github.com/frk1/hazedumper) so thank you to frk1, rN' and the other contributors to that project.

I started learning Rust when messing with HazeDumper and I decided we needed a C++ version, I also wanted to extend the functionality.

GH Dumper will do the same thing as HazeDumper with the addition of dumping ReClass files and Cheat Engine Tables.

Our dumper uses the same json config file format, so they are interchangeable

### Notes
Ignore the files in the modules folder, these are just modules from my framework which I use to do some of the background stuff.

The main code is GHDumper and SrcDumper

### TODO
* Reduce bloat from modules folder
* Improve CE Output
* Add ReClass.NET output
* Turn into a lib which can be incorporated into hacks
* Make an internal version
* Seperate CSGO vs regular functionality
* Add CSS functionality
* organize output of offsets or put comments showing what module they are for and what base object
* Other ideas to make it kewl

### Credits
Thank you to frk1, rN' and the contributors to [hazedumper](https://github.com/frk1/hazedumper)

Thank you to hjiang and the contributors of [jsonxx](https://github.com/hjiang/jsonxx/)

<h3>Official GH Courses</h3>
<ul>
	<li><a href="https://guidedhacking.com/ghb" target="_blank">The Game Hacking Bible</a>&nbsp;- a massive 70 chapter Game Hacking Course</li>
	<li><a href="https://guidedhacking.com/threads/squally-cs420-game-hacking-course.14191/" target="_blank">Computer Science 420</a>&nbsp;- an eight chapter lecture on CS, Data Types &amp; Assembly</li>
	<li><a href="https://guidedhacking.com/forums/binary-exploit-development-course.551/" target="_blank">Binary Exploit Development</a>&nbsp;- a 9 chapter series on exploit dev&nbsp;from a certified OSED</li>
	<li><a href="https://guidedhacking.com/forums/game-hacking-shenanigans/" target="_blank">Game Hacking Shenanigans</a>&nbsp;- a twenty lesson Cheat Engine hacking course</li>
	<li><a href="https://guidedhacking.com/threads/python-game-hacking-tutorial-1-1-introduction.18695/" target="_blank">Python Game Hacking Course</a>&nbsp;- 7 chapter external &amp; internal python hack lesson</li>
	<li><a href="https://guidedhacking.com/threads/python-game-hacking-tutorial-2-1-introduction.19199/" target="_blank">Python App Reverse Engineering</a>&nbsp;- Learn to reverse python apps in 5 lessons</li>
	<li><a href="https://guidedhacking.com/threads/web-browser-game-hacking-intro-part-1.17726/" target="_blank">Web Browser Game Hacking</a>&nbsp;- Hack javascript games with this 4 chapter course</li>
	<li><a href="https://guidedhacking.com/forums/roblox-exploit-scripting-course-res100.521/" target="_blank">Roblox Exploiting Course</a>&nbsp;- 7 Premium Lessons on Hacking Roblox</li>
	<li><a href="https://guidedhacking.com/forums/java-reverse-engineering-course-jre100.538/" target="_blank">Java Reverse Engineering Course</a>&nbsp;- 5 chapter beginner guide</li>
	<li><a href="https://guidedhacking.com/forums/java-game-hacking-course-jgh100.553/" target="_blank">Java Game Hacking Course</a>&nbsp;- 6 Chapter Beginner Guide</li>
</ul>
