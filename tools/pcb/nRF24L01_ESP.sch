<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<!--Created by TARGET 3001! Version: 30.8.0.19-->
<!--DateTime: 2022.10.16 11:23:02-->
<eagle version="7.5.0">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.635" unitdist="mm" unit="mm" style="lines" multiple="2" display="no" altdistance="0.635" altunitdist="mm" altunit="mm"/>
<layers>
<layer number="90" name="Modules" color="5" fill="1" visible="yes" active="yes"/>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="no" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<libraries>
<library name="TARGET3001!_V30.8.0.19">
<packages>
<package name="STIFTLEISTE_2X04_G_2,54_0">
<wire x1="0.635" y1="-1.27" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-1.27" y1="-1.27" x2="-1.27" y2="3.175" width="0.3" layer="21"/>
<pad name="1" x="0" y="0" drill="1" shape="square" diameter="1.6"/>
<pad name="2" x="0" y="2.54" drill="1" shape="octagon" diameter="1.6"/>
<pad name="3" x="2.54" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="4" x="2.54" y="2.54" drill="1" shape="octagon" diameter="1.6"/>
<pad name="5" x="5.08" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="6" x="5.08" y="2.54" drill="1" shape="octagon" diameter="1.6"/>
<pad name="7" x="7.62" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="8" x="7.62" y="2.54" drill="1" shape="octagon" diameter="1.6"/>
<wire x1="0.635" y1="-1.27" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="-0.635" x2="1.905" y2="-1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="-1.27" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="-0.635" x2="4.445" y2="-1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="-1.27" x2="6.35" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="-1.27" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="-1.27" y1="3.175" x2="-0.635" y2="3.81" width="0.3" layer="21"/>
<wire x1="-0.635" y1="3.81" x2="0.635" y2="3.81" width="0.3" layer="21"/>
<wire x1="0.635" y1="3.81" x2="1.27" y2="3.175" width="0.3" layer="21"/>
<wire x1="1.27" y1="3.175" x2="1.905" y2="3.81" width="0.3" layer="21"/>
<wire x1="1.905" y1="3.81" x2="3.175" y2="3.81" width="0.3" layer="21"/>
<wire x1="3.175" y1="3.81" x2="3.81" y2="3.175" width="0.3" layer="21"/>
<wire x1="3.81" y1="3.175" x2="4.445" y2="3.81" width="0.3" layer="21"/>
<wire x1="4.445" y1="3.81" x2="5.715" y2="3.81" width="0.3" layer="21"/>
<wire x1="5.715" y1="3.81" x2="6.35" y2="3.175" width="0.3" layer="21"/>
<wire x1="6.35" y1="3.175" x2="6.985" y2="3.81" width="0.3" layer="21"/>
<wire x1="6.985" y1="3.81" x2="8.255" y2="3.81" width="0.3" layer="21"/>
<wire x1="8.255" y1="3.81" x2="8.89" y2="3.175" width="0.3" layer="21"/>
<wire x1="1.27" y1="3.175" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="3.175" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="3.175" x2="6.35" y2="-0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="3.175" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.985" y1="-1.27" x2="8.255" y2="-1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="-1.27" x2="5.715" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="-1.27" x2="3.175" y2="-1.27" width="0.3" layer="21"/>
<text x="0.635" y="4.445" size="2" layer="27" font="vector" rot="R90">&gt;VALUE</text>
</package>
<package name="KLEMME4_3">
<pad name="1" x="-7.62" y="0" drill="1.3" shape="octagon" diameter="2.3"/>
<pad name="2" x="-2.54" y="0" drill="1.3" shape="octagon" diameter="2.3"/>
<pad name="3" x="2.54" y="0" drill="1.3" shape="octagon" diameter="2.3"/>
<pad name="4" x="7.62" y="0" drill="1.3" shape="octagon" diameter="2.3"/>
<wire x1="-10.16" y1="4.05" x2="10.16" y2="4.05" width="0.3" layer="21"/>
<circle x="-7.62" y="0" radius="1.905" width="0.3" layer="21"/>
<circle x="-2.54" y="0" radius="1.905" width="0.3" layer="21"/>
<circle x="2.54" y="0" radius="1.905" width="0.3" layer="21"/>
<circle x="7.62" y="0" radius="1.905" width="0.3" layer="21"/>
<wire x1="10.084" y1="-3" x2="-10.084" y2="-3" width="0.3" layer="21"/>
<wire x1="-10.084" y1="3" x2="10.084" y2="3" width="0.3" layer="21"/>
<wire x1="6.668" y1="-1.589" x2="9.208" y2="0.951" width="0.3" layer="21"/>
<wire x1="6.032" y1="-0.953" x2="8.572" y2="1.587" width="0.3" layer="21"/>
<wire x1="1.588" y1="-1.589" x2="4.128" y2="0.951" width="0.3" layer="21"/>
<wire x1="0.952" y1="-0.953" x2="3.492" y2="1.587" width="0.3" layer="21"/>
<wire x1="-3.492" y1="-1.589" x2="-0.952" y2="0.951" width="0.3" layer="21"/>
<wire x1="-4.128" y1="-0.953" x2="-1.588" y2="1.587" width="0.3" layer="21"/>
<wire x1="-8.572" y1="-1.589" x2="-6.032" y2="0.951" width="0.3" layer="21"/>
<wire x1="-9.208" y1="-0.953" x2="-6.668" y2="1.587" width="0.3" layer="21"/>
<wire x1="-10.16" y1="-4.05" x2="10.16" y2="-4.05" width="0.3" layer="21"/>
<wire x1="-10.16" y1="4.05" x2="-10.16" y2="-4.05" width="0.3" layer="21"/>
<wire x1="10.16" y1="4.05" x2="10.16" y2="-4.05" width="0.3" layer="21"/>
<text x="12.795" y="1.905" size="2" layer="27" font="vector" rot="R90" align="bottom-right">&gt;VALUE</text>
</package>
<package name="STIFTLEISTE_1X08_G_2,54_4">
<wire x1="-0.635" y1="1.27" x2="0.635" y2="1.27" width="0.3" layer="21"/>
<wire x1="0.635" y1="1.27" x2="1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="-0.635" x2="0.635" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-1.27" y1="0.635" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-0.635" y1="1.27" x2="-1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="0.635" y1="-1.27" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<pad name="1" x="0" y="0" drill="1" shape="square" diameter="1.6"/>
<pad name="2" x="2.54" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="3" x="5.08" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="4" x="7.62" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="5" x="10.16" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="6" x="12.7" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="7" x="15.24" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="8" x="17.78" y="0" drill="1" shape="octagon" diameter="1.6"/>
<wire x1="14.605" y1="1.27" x2="15.875" y2="1.27" width="0.3" layer="21"/>
<wire x1="15.875" y1="1.27" x2="16.51" y2="0.635" width="0.3" layer="21"/>
<wire x1="16.51" y1="0.635" x2="16.51" y2="-0.635" width="0.3" layer="21"/>
<wire x1="16.51" y1="-0.635" x2="15.875" y2="-1.27" width="0.3" layer="21"/>
<wire x1="11.43" y1="0.635" x2="12.065" y2="1.27" width="0.3" layer="21"/>
<wire x1="12.065" y1="1.27" x2="13.335" y2="1.27" width="0.3" layer="21"/>
<wire x1="13.335" y1="1.27" x2="13.97" y2="0.635" width="0.3" layer="21"/>
<wire x1="13.97" y1="0.635" x2="13.97" y2="-0.635" width="0.3" layer="21"/>
<wire x1="13.97" y1="-0.635" x2="13.335" y2="-1.27" width="0.3" layer="21"/>
<wire x1="13.335" y1="-1.27" x2="12.065" y2="-1.27" width="0.3" layer="21"/>
<wire x1="12.065" y1="-1.27" x2="11.43" y2="-0.635" width="0.3" layer="21"/>
<wire x1="14.605" y1="1.27" x2="13.97" y2="0.635" width="0.3" layer="21"/>
<wire x1="13.97" y1="-0.635" x2="14.605" y2="-1.27" width="0.3" layer="21"/>
<wire x1="15.875" y1="-1.27" x2="14.605" y2="-1.27" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="8.255" y2="1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="1.27" x2="8.89" y2="0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="0.635" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="-0.635" x2="8.255" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.89" y1="0.635" x2="9.525" y2="1.27" width="0.3" layer="21"/>
<wire x1="9.525" y1="1.27" x2="10.795" y2="1.27" width="0.3" layer="21"/>
<wire x1="10.795" y1="1.27" x2="11.43" y2="0.635" width="0.3" layer="21"/>
<wire x1="11.43" y1="0.635" x2="11.43" y2="-0.635" width="0.3" layer="21"/>
<wire x1="11.43" y1="-0.635" x2="10.795" y2="-1.27" width="0.3" layer="21"/>
<wire x1="10.795" y1="-1.27" x2="9.525" y2="-1.27" width="0.3" layer="21"/>
<wire x1="9.525" y1="-1.27" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="4.445" y2="1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="1.27" x2="5.715" y2="1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="0.635" x2="6.35" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="5.715" y2="-1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="-1.27" x2="4.445" y2="-1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="-1.27" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="-1.27" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.905" y2="1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="1.27" x2="3.175" y2="1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="1.27" x2="3.81" y2="0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="-0.635" x2="3.175" y2="-1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="-1.27" x2="1.905" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="-1.27" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="17.145" y1="1.27" x2="18.415" y2="1.27" width="0.3" layer="21"/>
<wire x1="18.415" y1="1.27" x2="19.05" y2="0.635" width="0.3" layer="21"/>
<wire x1="19.05" y1="0.635" x2="19.05" y2="-0.635" width="0.3" layer="21"/>
<wire x1="19.05" y1="-0.635" x2="18.415" y2="-1.27" width="0.3" layer="21"/>
<wire x1="17.145" y1="1.27" x2="16.51" y2="0.635" width="0.3" layer="21"/>
<wire x1="16.51" y1="-0.635" x2="17.145" y2="-1.27" width="0.3" layer="21"/>
<wire x1="18.415" y1="-1.27" x2="17.145" y2="-1.27" width="0.3" layer="21"/>
<text x="-1.27" y="1.905" size="2" layer="27" font="vector">&gt;VALUE</text>
</package>
<package name="STIFTLEISTE_1X08_G_2,54_5">
<wire x1="-0.635" y1="1.27" x2="0.635" y2="1.27" width="0.3" layer="21"/>
<wire x1="0.635" y1="1.27" x2="1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="-0.635" x2="0.635" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-1.27" y1="0.635" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-0.635" y1="1.27" x2="-1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="0.635" y1="-1.27" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<pad name="1" x="0" y="0" drill="1" shape="square" diameter="1.6"/>
<pad name="2" x="2.54" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="3" x="5.08" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="4" x="7.62" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="5" x="10.16" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="6" x="12.7" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="7" x="15.24" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="8" x="17.78" y="0" drill="1" shape="octagon" diameter="1.6"/>
<wire x1="14.605" y1="1.27" x2="15.875" y2="1.27" width="0.3" layer="21"/>
<wire x1="15.875" y1="1.27" x2="16.51" y2="0.635" width="0.3" layer="21"/>
<wire x1="16.51" y1="0.635" x2="16.51" y2="-0.635" width="0.3" layer="21"/>
<wire x1="16.51" y1="-0.635" x2="15.875" y2="-1.27" width="0.3" layer="21"/>
<wire x1="11.43" y1="0.635" x2="12.065" y2="1.27" width="0.3" layer="21"/>
<wire x1="12.065" y1="1.27" x2="13.335" y2="1.27" width="0.3" layer="21"/>
<wire x1="13.335" y1="1.27" x2="13.97" y2="0.635" width="0.3" layer="21"/>
<wire x1="13.97" y1="0.635" x2="13.97" y2="-0.635" width="0.3" layer="21"/>
<wire x1="13.97" y1="-0.635" x2="13.335" y2="-1.27" width="0.3" layer="21"/>
<wire x1="13.335" y1="-1.27" x2="12.065" y2="-1.27" width="0.3" layer="21"/>
<wire x1="12.065" y1="-1.27" x2="11.43" y2="-0.635" width="0.3" layer="21"/>
<wire x1="14.605" y1="1.27" x2="13.97" y2="0.635" width="0.3" layer="21"/>
<wire x1="13.97" y1="-0.635" x2="14.605" y2="-1.27" width="0.3" layer="21"/>
<wire x1="15.875" y1="-1.27" x2="14.605" y2="-1.27" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="8.255" y2="1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="1.27" x2="8.89" y2="0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="0.635" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="-0.635" x2="8.255" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.89" y1="0.635" x2="9.525" y2="1.27" width="0.3" layer="21"/>
<wire x1="9.525" y1="1.27" x2="10.795" y2="1.27" width="0.3" layer="21"/>
<wire x1="10.795" y1="1.27" x2="11.43" y2="0.635" width="0.3" layer="21"/>
<wire x1="11.43" y1="0.635" x2="11.43" y2="-0.635" width="0.3" layer="21"/>
<wire x1="11.43" y1="-0.635" x2="10.795" y2="-1.27" width="0.3" layer="21"/>
<wire x1="10.795" y1="-1.27" x2="9.525" y2="-1.27" width="0.3" layer="21"/>
<wire x1="9.525" y1="-1.27" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="4.445" y2="1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="1.27" x2="5.715" y2="1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="0.635" x2="6.35" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="5.715" y2="-1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="-1.27" x2="4.445" y2="-1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="-1.27" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="-1.27" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.905" y2="1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="1.27" x2="3.175" y2="1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="1.27" x2="3.81" y2="0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="-0.635" x2="3.175" y2="-1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="-1.27" x2="1.905" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="-1.27" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="17.145" y1="1.27" x2="18.415" y2="1.27" width="0.3" layer="21"/>
<wire x1="18.415" y1="1.27" x2="19.05" y2="0.635" width="0.3" layer="21"/>
<wire x1="19.05" y1="0.635" x2="19.05" y2="-0.635" width="0.3" layer="21"/>
<wire x1="19.05" y1="-0.635" x2="18.415" y2="-1.27" width="0.3" layer="21"/>
<wire x1="17.145" y1="1.27" x2="16.51" y2="0.635" width="0.3" layer="21"/>
<wire x1="16.51" y1="-0.635" x2="17.145" y2="-1.27" width="0.3" layer="21"/>
<wire x1="18.415" y1="-1.27" x2="17.145" y2="-1.27" width="0.3" layer="21"/>
<text x="-1.27" y="-3.81" size="2" layer="27" font="vector">&gt;VALUE</text>
</package>
<package name="STIFTLEISTE_1X02_G_2,54_10">
<pad name="1" x="0" y="0" drill="1" shape="square" diameter="1.6"/>
<pad name="2" x="2.54" y="0" drill="1" shape="octagon" diameter="1.6"/>
<wire x1="-0.635" y1="1.27" x2="0.635" y2="1.27" width="0.3" layer="21"/>
<wire x1="0.635" y1="1.27" x2="1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="-0.635" x2="0.635" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-1.27" y1="0.635" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-0.635" y1="1.27" x2="-1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="0.635" y1="-1.27" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.905" y2="1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="1.27" x2="3.175" y2="1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="1.27" x2="3.81" y2="0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="-0.635" x2="3.175" y2="-1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="-1.27" x2="1.905" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="-1.27" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<text x="6.35" y="1.675" size="1.5" layer="27" font="vector" align="bottom-right">&gt;VALUE</text>
</package>
<package name="STIFTLEISTE_1X04_G_2,54_11">
<wire x1="-0.635" y1="1.27" x2="0.635" y2="1.27" width="0.3" layer="21"/>
<wire x1="0.635" y1="1.27" x2="1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="-0.635" x2="0.635" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-1.27" y1="0.635" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-0.635" y1="1.27" x2="-1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="0.635" y1="-1.27" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<pad name="1" x="0" y="0" drill="1" shape="square" diameter="1.6"/>
<pad name="2" x="2.54" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="3" x="5.08" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="4" x="7.62" y="0" drill="1" shape="octagon" diameter="1.6"/>
<wire x1="3.81" y1="0.635" x2="4.445" y2="1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="1.27" x2="5.715" y2="1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="0.635" x2="6.35" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="5.715" y2="-1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="-1.27" x2="4.445" y2="-1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="-1.27" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.905" y2="1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="1.27" x2="3.175" y2="1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="1.27" x2="3.81" y2="0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="-0.635" x2="3.175" y2="-1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="-1.27" x2="1.905" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="-1.27" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="8.255" y2="1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="1.27" x2="8.89" y2="0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="0.635" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="-0.635" x2="8.255" y2="-1.27" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="-1.27" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<text x="-1.27" y="-3.81" size="2" layer="27" font="vector">&gt;VALUE</text>
</package>
<package name="STIFTLEISTE_1X04_G_2,54_13">
<wire x1="-0.635" y1="1.27" x2="0.635" y2="1.27" width="0.3" layer="21"/>
<wire x1="0.635" y1="1.27" x2="1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="-0.635" x2="0.635" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-1.27" y1="0.635" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<wire x1="-0.635" y1="1.27" x2="-1.27" y2="0.635" width="0.3" layer="21"/>
<wire x1="0.635" y1="-1.27" x2="-1.27" y2="-1.27" width="0.3" layer="21"/>
<pad name="1" x="0" y="0" drill="1" shape="square" diameter="1.6"/>
<pad name="2" x="2.54" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="3" x="5.08" y="0" drill="1" shape="octagon" diameter="1.6"/>
<pad name="4" x="7.62" y="0" drill="1" shape="octagon" diameter="1.6"/>
<wire x1="3.81" y1="0.635" x2="4.445" y2="1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="1.27" x2="5.715" y2="1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="0.635" x2="6.35" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="5.715" y2="-1.27" width="0.3" layer="21"/>
<wire x1="5.715" y1="-1.27" x2="4.445" y2="-1.27" width="0.3" layer="21"/>
<wire x1="4.445" y1="-1.27" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="1.27" y1="0.635" x2="1.905" y2="1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="1.27" x2="3.175" y2="1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="1.27" x2="3.81" y2="0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="0.635" x2="3.81" y2="-0.635" width="0.3" layer="21"/>
<wire x1="3.81" y1="-0.635" x2="3.175" y2="-1.27" width="0.3" layer="21"/>
<wire x1="3.175" y1="-1.27" x2="1.905" y2="-1.27" width="0.3" layer="21"/>
<wire x1="1.905" y1="-1.27" x2="1.27" y2="-0.635" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="8.255" y2="1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="1.27" x2="8.89" y2="0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="0.635" x2="8.89" y2="-0.635" width="0.3" layer="21"/>
<wire x1="8.89" y1="-0.635" x2="8.255" y2="-1.27" width="0.3" layer="21"/>
<wire x1="6.985" y1="1.27" x2="6.35" y2="0.635" width="0.3" layer="21"/>
<wire x1="6.35" y1="-0.635" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<wire x1="8.255" y1="-1.27" x2="6.985" y2="-1.27" width="0.3" layer="21"/>
<text x="1.27" y="-3.81" size="2" layer="27" font="vector">&gt;VALUE</text>
</package>
</packages>
<symbols>
<symbol name="STIFTLEISTE_2X04_G_2,54_0">
<text x="-1.27" y="5.715" size="2" layer="95">&gt;NAME</text>
<pin name="2" x="3.81" y="3.81" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="3" x="-3.81" y="1.27" visible="pad" length="short" direction="pas"/>
<pin name="4" x="3.81" y="1.27" visible="pad" length="short" direction="pas" rot="R180"/>
<wire x1="-1.27" y1="5.08" x2="-1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="-5.08" x2="-1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="5.08" x2="1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="5.08" x2="-1.27" y2="5.08" width="0.3" layer="94"/>
<text x="-1.27" y="-7.62" size="2" layer="96">&gt;VALUE</text>
<pin name="1" x="-3.81" y="3.81" visible="pad" length="short" direction="pas"/>
<pin name="6" x="3.81" y="-1.27" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="7" x="-3.81" y="-3.81" visible="pad" length="short" direction="pas"/>
<pin name="8" x="3.81" y="-3.81" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="5" x="-3.81" y="-1.27" visible="pad" length="short" direction="pas"/>
</symbol>
<symbol name="KLEMME4POL_3">
<wire x1="-1.27" y1="6.35" x2="-1.27" y2="-6.35" width="0.3" layer="94"/>
<wire x1="1.27" y1="6.35" x2="-1.27" y2="6.35" width="0.3" layer="94"/>
<wire x1="1.27" y1="6.35" x2="1.27" y2="-6.35" width="0.3" layer="94"/>
<wire x1="1.27" y1="-6.35" x2="-1.27" y2="-6.35" width="0.3" layer="94"/>
<text x="-1.27" y="-8.89" size="2" layer="96">&gt;VALUE</text>
<text x="-1.27" y="6.985" size="2" layer="95">&gt;NAME</text>
<pin name="1" x="3.81" y="3.81" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="2" x="3.81" y="1.27" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="3" x="3.81" y="-1.27" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="4" x="3.81" y="-3.81" visible="pad" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="STIFTLEISTE_1X08_G_2,54_4">
<text x="-1.27" y="-12.7" size="2" layer="96">&gt;VALUE</text>
<pin name="1" x="-3.81" y="8.89" visible="pad" length="short" direction="pas"/>
<pin name="2" x="-3.81" y="6.35" visible="pad" length="short" direction="pas"/>
<pin name="3" x="-3.81" y="3.81" visible="pad" length="short" direction="pas"/>
<pin name="4" x="-3.81" y="1.27" visible="pad" length="short" direction="pas"/>
<text x="-1.27" y="10.795" size="2" layer="95">&gt;NAME</text>
<wire x1="-1.27" y1="10.16" x2="-1.27" y2="-10.16" width="0.3" layer="94"/>
<wire x1="1.27" y1="-10.16" x2="-1.27" y2="-10.16" width="0.3" layer="94"/>
<wire x1="1.27" y1="10.16" x2="1.27" y2="-10.16" width="0.3" layer="94"/>
<wire x1="1.27" y1="10.16" x2="-1.27" y2="10.16" width="0.3" layer="94"/>
<pin name="5" x="-3.81" y="-1.27" visible="pad" length="short" direction="pas"/>
<pin name="6" x="-3.81" y="-3.81" visible="pad" length="short" direction="pas"/>
<pin name="7" x="-3.81" y="-6.35" visible="pad" length="short" direction="pas"/>
<pin name="8" x="-3.81" y="-8.89" visible="pad" length="short" direction="pas"/>
</symbol>
<symbol name="STIFTLEISTE_1X08_G_2,54_5">
<text x="-1.27" y="-12.7" size="2" layer="96">&gt;VALUE</text>
<pin name="1" x="-3.81" y="8.89" visible="pad" length="short" direction="pas"/>
<pin name="2" x="-3.81" y="6.35" visible="pad" length="short" direction="pas"/>
<pin name="3" x="-3.81" y="3.81" visible="pad" length="short" direction="pas"/>
<pin name="4" x="-3.81" y="1.27" visible="pad" length="short" direction="pas"/>
<text x="-1.27" y="10.795" size="2" layer="95">&gt;NAME</text>
<wire x1="-1.27" y1="10.16" x2="-1.27" y2="-10.16" width="0.3" layer="94"/>
<wire x1="1.27" y1="-10.16" x2="-1.27" y2="-10.16" width="0.3" layer="94"/>
<wire x1="1.27" y1="10.16" x2="1.27" y2="-10.16" width="0.3" layer="94"/>
<wire x1="1.27" y1="10.16" x2="-1.27" y2="10.16" width="0.3" layer="94"/>
<pin name="5" x="-3.81" y="-1.27" visible="pad" length="short" direction="pas"/>
<pin name="6" x="-3.81" y="-3.81" visible="pad" length="short" direction="pas"/>
<pin name="7" x="-3.81" y="-6.35" visible="pad" length="short" direction="pas"/>
<pin name="8" x="-3.81" y="-8.89" visible="pad" length="short" direction="pas"/>
</symbol>
<symbol name="STIFTLEISTE_1X02_G_2,54_10">
<text x="-1.27" y="-5.08" size="2" layer="96">&gt;VALUE</text>
<pin name="1" x="-3.81" y="1.27" visible="pad" length="short" direction="pas"/>
<pin name="2" x="-3.81" y="-1.27" visible="pad" length="short" direction="pas"/>
<text x="-1.27" y="3.175" size="2" layer="95">&gt;NAME</text>
<wire x1="1.27" y1="-2.54" x2="-1.27" y2="-2.54" width="0.3" layer="94"/>
<wire x1="-1.27" y1="2.54" x2="-1.27" y2="-2.54" width="0.3" layer="94"/>
<wire x1="1.27" y1="2.54" x2="-1.27" y2="2.54" width="0.3" layer="94"/>
<wire x1="1.27" y1="2.54" x2="1.27" y2="-2.54" width="0.3" layer="94"/>
</symbol>
<symbol name="STIFTLEISTE_1X04_G_2,54_11">
<text x="-1.27" y="-7.62" size="2" layer="96">&gt;VALUE</text>
<pin name="1" x="-3.81" y="3.81" visible="pad" length="short" direction="pas"/>
<pin name="2" x="-3.81" y="1.27" visible="pad" length="short" direction="pas"/>
<pin name="3" x="-3.81" y="-1.27" visible="pad" length="short" direction="pas"/>
<pin name="4" x="-3.81" y="-3.81" visible="pad" length="short" direction="pas"/>
<text x="-1.27" y="5.715" size="2" layer="95">&gt;NAME</text>
<wire x1="-1.27" y1="5.08" x2="-1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="-5.08" x2="-1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="5.08" x2="1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="5.08" x2="-1.27" y2="5.08" width="0.3" layer="94"/>
</symbol>
<symbol name="STIFTLEISTE_1X04_G_2,54_13">
<text x="-1.27" y="-7.62" size="2" layer="96">&gt;VALUE</text>
<pin name="1" x="-3.81" y="3.81" visible="pad" length="short" direction="pas"/>
<pin name="2" x="-3.81" y="1.27" visible="pad" length="short" direction="pas"/>
<pin name="3" x="-3.81" y="-1.27" visible="pad" length="short" direction="pas"/>
<pin name="4" x="-3.81" y="-3.81" visible="pad" length="short" direction="pas"/>
<text x="-1.27" y="5.715" size="2" layer="95">&gt;NAME</text>
<wire x1="-1.27" y1="5.08" x2="-1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="-5.08" x2="-1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="5.08" x2="1.27" y2="-5.08" width="0.3" layer="94"/>
<wire x1="1.27" y1="5.08" x2="-1.27" y2="5.08" width="0.3" layer="94"/>
</symbol>
</symbols>
<devicesets>
<deviceset name="STIFTLEISTE_2X04_G_2,54_0" prefix="K">
<description>Stiftleiste</description>
<gates>
<gate name="G$1" symbol="STIFTLEISTE_2X04_G_2,54_0" x="-99.06" y="73.025"/>
</gates>
<devices>
<device name="" package="STIFTLEISTE_2X04_G_2,54_0">
<connects>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="6" pad="6"/>
<connect gate="G$1" pin="7" pad="7"/>
<connect gate="G$1" pin="8" pad="8"/>
<connect gate="G$1" pin="5" pad="5"/>
</connects>
</device>
</devices>
</deviceset>
<deviceset name="KLEMME4POL_3" prefix="K">
<description>Schraubklemme 4-polig RM5,08</description>
<gates>
<gate name="G$1" symbol="KLEMME4POL_3" x="-146.05" y="48.26"/>
</gates>
<devices>
<device name="" package="KLEMME4_3">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
</connects>
</device>
</devices>
</deviceset>
<deviceset name="STIFTLEISTE_1X08_G_2,54_4" prefix="K">
<description>Stiftleiste</description>
<gates>
<gate name="G$1" symbol="STIFTLEISTE_1X08_G_2,54_4" x="-108.585" y="42.545"/>
</gates>
<devices>
<device name="" package="STIFTLEISTE_1X08_G_2,54_4">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
<connect gate="G$1" pin="5" pad="5"/>
<connect gate="G$1" pin="6" pad="6"/>
<connect gate="G$1" pin="7" pad="7"/>
<connect gate="G$1" pin="8" pad="8"/>
</connects>
</device>
</devices>
</deviceset>
<deviceset name="STIFTLEISTE_1X08_G_2,54_5" prefix="K">
<description>Stiftleiste</description>
<gates>
<gate name="G$1" symbol="STIFTLEISTE_1X08_G_2,54_5" x="-85.725" y="42.545"/>
</gates>
<devices>
<device name="" package="STIFTLEISTE_1X08_G_2,54_5">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
<connect gate="G$1" pin="5" pad="5"/>
<connect gate="G$1" pin="6" pad="6"/>
<connect gate="G$1" pin="7" pad="7"/>
<connect gate="G$1" pin="8" pad="8"/>
</connects>
</device>
</devices>
</deviceset>
<deviceset name="STIFTLEISTE_1X02_G_2,54_10" prefix="K">
<description>Stiftleiste</description>
<gates>
<gate name="G$1" symbol="STIFTLEISTE_1X02_G_2,54_10" x="-134.62" y="69.215"/>
</gates>
<devices>
<device name="" package="STIFTLEISTE_1X02_G_2,54_10">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
</device>
</devices>
</deviceset>
<deviceset name="STIFTLEISTE_1X04_G_2,54_11" prefix="K">
<description>Stiftleiste</description>
<gates>
<gate name="G$1" symbol="STIFTLEISTE_1X04_G_2,54_11" x="-47.625" y="62.23"/>
</gates>
<devices>
<device name="" package="STIFTLEISTE_1X04_G_2,54_11">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
</connects>
</device>
</devices>
</deviceset>
<deviceset name="STIFTLEISTE_1X04_G_2,54_13" prefix="K">
<description>Stiftleiste</description>
<gates>
<gate name="G$1" symbol="STIFTLEISTE_1X04_G_2,54_13" x="-47.625" y="41.91"/>
</gates>
<devices>
<device name="" package="STIFTLEISTE_1X04_G_2,54_13">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
</connects>
</device>
</devices>
</deviceset>
</devicesets>
</library>
</libraries>
<attributes/>
<variantdefs>
<variantdef name="&lt;unbenannt&gt;" current="yes"/>
</variantdefs>
<classes>
<class number="0" name="default" width="0" drill="0"/>
</classes>
<parts>
<part name="K1" library="TARGET3001!_V30.8.0.19" deviceset="STIFTLEISTE_2X04_G_2,54_0" device="" value="nRF24L01+"/>
<part name="K3" library="TARGET3001!_V30.8.0.19" deviceset="KLEMME4POL_3" device="" value="5V DC"/>
<part name="K4" library="TARGET3001!_V30.8.0.19" deviceset="STIFTLEISTE_1X08_G_2,54_4" device="" value="LINKS"/>
<part name="K5" library="TARGET3001!_V30.8.0.19" deviceset="STIFTLEISTE_1X08_G_2,54_5" device="" value="RECHTS"/>
<part name="K2" library="TARGET3001!_V30.8.0.19" deviceset="STIFTLEISTE_1X02_G_2,54_10" device="" value="RST to D0"/>
<part name="K6" library="TARGET3001!_V30.8.0.19" deviceset="STIFTLEISTE_1X04_G_2,54_11" device="" value="UART 3V3"/>
<part name="K7" library="TARGET3001!_V30.8.0.19" deviceset="STIFTLEISTE_1X04_G_2,54_13" device="" value="I2C 3V3"/>
</parts>
<sheets>
<sheet>
<description>&lt;unbenannt&gt;</description>
<plain>
<text x="-83.82" y="45.695" size="1.75" layer="97">D1 SDA</text>
<text x="-106.045" y="45.695" size="1.75" layer="97">D0</text>
<text x="-106.045" y="50.945" size="1.75" layer="97">RST</text>
<text x="-106.045" y="48.32" size="1.75" layer="97">A0</text>
<text x="-106.045" y="43.07" size="1.75" layer="97">D5</text>
<text x="-106.045" y="40.445" size="1.75" layer="97">D6</text>
<text x="-106.045" y="37.82" size="1.75" layer="97">D7</text>
<text x="-106.045" y="35.195" size="1.75" layer="97">D8</text>
<text x="-106.045" y="32.57" size="1.75" layer="97">3V3</text>
<text x="-83.82" y="50.945" size="1.75" layer="97">TX</text>
<text x="-83.82" y="48.32" size="1.75" layer="97">RX</text>
<text x="-83.82" y="43.07" size="1.75" layer="97">D2 SCL</text>
<text x="-83.82" y="40.445" size="1.75" layer="97">D3</text>
<text x="-83.82" y="37.82" size="1.75" layer="97">D4</text>
<text x="-83.82" y="35.195" size="1.75" layer="97">G</text>
<text x="-83.82" y="32.57" size="1.75" layer="97">5V</text>
</plain>
<instances>
<instance part="K1" gate="G$1" x="-99.06" y="73.025"/>
<instance part="K3" gate="G$1" x="-146.05" y="48.26"/>
<instance part="K4" gate="G$1" x="-108.585" y="42.545"/>
<instance part="K5" gate="G$1" x="-85.725" y="42.545"/>
<instance part="K2" gate="G$1" x="-134.62" y="69.215" rot="R180"/>
<instance part="K6" gate="G$1" x="-47.625" y="62.23"/>
<instance part="K7" gate="G$1" x="-47.625" y="41.91"/>
</instances>
<busses/>
<nets>
<net name="GND" class="0">
<segment>
<wire x1="-106.045" y1="76.835" x2="-106.045" y2="80.01" width="0.3" layer="91"/>
<label x="-106.045" y="80.01" size="1.5" layer="95" rot="R90"/>
<junction x="-106.045" y="80.01"/>
<wire x1="-100.965" y1="80.01" x2="-106.045" y2="80.01" width="0.3" layer="91"/>
<wire x1="-102.87" y1="76.835" x2="-106.045" y2="76.835" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="1"/>
<wire x1="-100.965" y1="85.725" x2="-100.965" y2="80.01" width="0.3" layer="91"/>
<wire x1="-74.93" y1="85.725" x2="-100.965" y2="85.725" width="0.3" layer="91"/>
<wire x1="-74.93" y1="58.42" x2="-74.93" y2="85.725" width="0.3" layer="91"/>
<wire x1="-58.42" y1="58.42" x2="-74.93" y2="58.42" width="0.3" layer="91"/>
<junction x="-58.42" y="58.42"/>
<wire x1="-58.42" y1="58.42" x2="-51.435" y2="58.42" width="0.3" layer="91"/>
<pinref part="K6" gate="G$1" pin="4"/>
<wire x1="-58.42" y1="38.1" x2="-58.42" y2="58.42" width="0.3" layer="91"/>
<wire x1="-51.435" y1="38.1" x2="-58.42" y2="38.1" width="0.3" layer="91"/>
<pinref part="K7" gate="G$1" pin="4"/>
</segment>
<segment>
<wire x1="-142.24" y1="44.45" x2="-136.525" y2="44.45" width="0.3" layer="91"/>
<label x="-136.525" y="44.45" size="1.5" layer="95" rot="R270"/>
<pinref part="K3" gate="G$1" pin="4"/>
<junction x="-136.525" y="44.45"/>
<wire x1="-136.525" y1="46.99" x2="-136.525" y2="44.45" width="0.3" layer="91"/>
<wire x1="-142.24" y1="46.99" x2="-136.525" y2="46.99" width="0.3" layer="91"/>
<pinref part="K3" gate="G$1" pin="3"/>
</segment>
<segment>
<wire x1="-96.52" y1="36.195" x2="-89.535" y2="36.195" width="0.3" layer="91"/>
<label x="-96.52" y="36.195" size="1.5" layer="95" rot="R270"/>
<pinref part="K5" gate="G$1" pin="7"/>
</segment>
</net>
<net name="+5V" class="0">
<segment>
<wire x1="-142.24" y1="52.07" x2="-136.525" y2="52.07" width="0.3" layer="91"/>
<label x="-136.525" y="52.07" size="1.5" layer="95" rot="R90"/>
<pinref part="K3" gate="G$1" pin="1"/>
<junction x="-136.525" y="52.07"/>
<wire x1="-136.525" y1="49.53" x2="-136.525" y2="52.07" width="0.3" layer="91"/>
<wire x1="-142.24" y1="49.53" x2="-136.525" y2="49.53" width="0.3" layer="91"/>
<pinref part="K3" gate="G$1" pin="2"/>
</segment>
<segment>
<wire x1="-89.535" y1="33.655" x2="-89.535" y2="27.94" width="0.3" layer="91"/>
<label x="-89.535" y="27.94" size="1.5" layer="95" rot="R270"/>
<pinref part="K5" gate="G$1" pin="8"/>
</segment>
</net>
<net name="+3V" class="0">
<segment>
<wire x1="-88.9" y1="80.01" x2="-88.9" y2="76.835" width="0.3" layer="91"/>
<label x="-88.9" y="80.01" size="1.5" layer="95" rot="R90"/>
<junction x="-88.9" y="80.01"/>
<wire x1="-88.9" y1="80.01" x2="-78.105" y2="80.01" width="0.3" layer="91"/>
<wire x1="-88.9" y1="76.835" x2="-95.25" y2="76.835" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="2"/>
<wire x1="-78.105" y1="80.01" x2="-78.105" y2="66.04" width="0.3" layer="91"/>
<wire x1="-53.975" y1="66.04" x2="-78.105" y2="66.04" width="0.3" layer="91"/>
<junction x="-53.975" y="66.04"/>
<wire x1="-53.975" y1="45.72" x2="-53.975" y2="66.04" width="0.3" layer="91"/>
<wire x1="-53.975" y1="66.04" x2="-51.435" y2="66.04" width="0.3" layer="91"/>
<pinref part="K6" gate="G$1" pin="1"/>
<wire x1="-51.435" y1="45.72" x2="-53.975" y2="45.72" width="0.3" layer="91"/>
<pinref part="K7" gate="G$1" pin="1"/>
</segment>
<segment>
<wire x1="-118.745" y1="33.655" x2="-112.395" y2="33.655" width="0.3" layer="91"/>
<label x="-118.745" y="33.655" size="1.5" layer="95" rot="R270"/>
<pinref part="K4" gate="G$1" pin="8"/>
</segment>
</net>
<net name="SIG$46" class="0">
<segment>
<wire x1="-102.87" y1="74.295" x2="-105.41" y2="74.295" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="3"/>
<wire x1="-105.41" y1="74.295" x2="-105.41" y2="55.88" width="0.3" layer="91"/>
<wire x1="-105.41" y1="55.88" x2="-100.33" y2="55.88" width="0.3" layer="91"/>
<wire x1="-100.33" y1="55.88" x2="-100.33" y2="38.735" width="0.3" layer="91"/>
<wire x1="-100.33" y1="38.735" x2="-89.535" y2="38.735" width="0.3" layer="91"/>
<pinref part="K5" gate="G$1" pin="6"/>
</segment>
</net>
<net name="SIG$47" class="0">
<segment>
<wire x1="-102.87" y1="71.755" x2="-114.935" y2="71.755" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="5"/>
<wire x1="-114.935" y1="71.755" x2="-114.935" y2="43.815" width="0.3" layer="91"/>
<wire x1="-114.935" y1="43.815" x2="-112.395" y2="43.815" width="0.3" layer="91"/>
<pinref part="K4" gate="G$1" pin="4"/>
</segment>
</net>
<net name="SIG$48" class="0">
<segment>
<wire x1="-102.87" y1="69.215" x2="-117.475" y2="69.215" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="7"/>
<wire x1="-117.475" y1="69.215" x2="-117.475" y2="41.275" width="0.3" layer="91"/>
<wire x1="-117.475" y1="41.275" x2="-112.395" y2="41.275" width="0.3" layer="91"/>
<pinref part="K4" gate="G$1" pin="5"/>
</segment>
</net>
<net name="SIG$49" class="0">
<segment>
<wire x1="-95.25" y1="74.295" x2="-92.71" y2="74.295" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="4"/>
<wire x1="-92.71" y1="74.295" x2="-92.71" y2="27.94" width="0.3" layer="91"/>
<wire x1="-92.71" y1="27.94" x2="-114.935" y2="27.94" width="0.3" layer="91"/>
<wire x1="-114.935" y1="27.94" x2="-114.935" y2="36.195" width="0.3" layer="91"/>
<wire x1="-114.935" y1="36.195" x2="-112.395" y2="36.195" width="0.3" layer="91"/>
<pinref part="K4" gate="G$1" pin="7"/>
</segment>
</net>
<net name="SIG$50" class="0">
<segment>
<wire x1="-95.25" y1="71.755" x2="-93.98" y2="71.755" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="6"/>
<wire x1="-93.98" y1="71.755" x2="-93.98" y2="63.5" width="0.3" layer="91"/>
<wire x1="-93.98" y1="63.5" x2="-120.015" y2="63.5" width="0.3" layer="91"/>
<wire x1="-120.015" y1="63.5" x2="-120.015" y2="38.735" width="0.3" layer="91"/>
<wire x1="-120.015" y1="38.735" x2="-112.395" y2="38.735" width="0.3" layer="91"/>
<pinref part="K4" gate="G$1" pin="6"/>
</segment>
</net>
<net name="SIG$51" class="0">
<segment>
<wire x1="-95.25" y1="69.215" x2="-93.345" y2="69.215" width="0.3" layer="91"/>
<pinref part="K1" gate="G$1" pin="8"/>
<wire x1="-93.345" y1="69.215" x2="-93.345" y2="60.96" width="0.3" layer="91"/>
<wire x1="-93.345" y1="60.96" x2="-95.25" y2="60.96" width="0.3" layer="91"/>
<wire x1="-95.25" y1="60.96" x2="-95.25" y2="41.275" width="0.3" layer="91"/>
<wire x1="-95.25" y1="41.275" x2="-89.535" y2="41.275" width="0.3" layer="91"/>
<pinref part="K5" gate="G$1" pin="5"/>
</segment>
</net>
<net name="SIG$57" class="0">
<segment>
<wire x1="-130.81" y1="70.485" x2="-126.365" y2="70.485" width="0.3" layer="91"/>
<pinref part="K2" gate="G$1" pin="2"/>
<wire x1="-126.365" y1="70.485" x2="-126.365" y2="51.435" width="0.3" layer="91"/>
<wire x1="-126.365" y1="51.435" x2="-112.395" y2="51.435" width="0.3" layer="91"/>
<pinref part="K4" gate="G$1" pin="1"/>
</segment>
</net>
<net name="SIG$58" class="0">
<segment>
<wire x1="-130.81" y1="67.945" x2="-128.905" y2="67.945" width="0.3" layer="91"/>
<pinref part="K2" gate="G$1" pin="1"/>
<wire x1="-128.905" y1="67.945" x2="-128.905" y2="46.355" width="0.3" layer="91"/>
<wire x1="-128.905" y1="46.355" x2="-112.395" y2="46.355" width="0.3" layer="91"/>
<pinref part="K4" gate="G$1" pin="3"/>
</segment>
</net>
<net name="SIG$59" class="0">
<segment>
<wire x1="-89.535" y1="51.435" x2="-90.805" y2="51.435" width="0.3" layer="91"/>
<pinref part="K5" gate="G$1" pin="1"/>
<wire x1="-90.805" y1="51.435" x2="-90.805" y2="57.15" width="0.3" layer="91"/>
<wire x1="-90.805" y1="57.15" x2="-78.105" y2="57.15" width="0.3" layer="91"/>
<wire x1="-78.105" y1="57.15" x2="-78.105" y2="63.5" width="0.3" layer="91"/>
<wire x1="-78.105" y1="63.5" x2="-51.435" y2="63.5" width="0.3" layer="91"/>
<pinref part="K6" gate="G$1" pin="2"/>
</segment>
</net>
<net name="SIG$60" class="0">
<segment>
<wire x1="-51.435" y1="60.96" x2="-92.075" y2="60.96" width="0.3" layer="91"/>
<pinref part="K6" gate="G$1" pin="3"/>
<wire x1="-92.075" y1="60.96" x2="-92.075" y2="48.895" width="0.3" layer="91"/>
<wire x1="-92.075" y1="48.895" x2="-89.535" y2="48.895" width="0.3" layer="91"/>
<pinref part="K5" gate="G$1" pin="2"/>
</segment>
</net>
<net name="SIG$64" class="0">
<segment>
<wire x1="-89.535" y1="46.355" x2="-93.345" y2="46.355" width="0.3" layer="91"/>
<pinref part="K5" gate="G$1" pin="3"/>
<wire x1="-93.345" y1="46.355" x2="-93.345" y2="53.975" width="0.3" layer="91"/>
<wire x1="-93.345" y1="53.975" x2="-74.295" y2="53.975" width="0.3" layer="91"/>
<wire x1="-74.295" y1="53.975" x2="-74.295" y2="43.18" width="0.3" layer="91"/>
<wire x1="-74.295" y1="43.18" x2="-51.435" y2="43.18" width="0.3" layer="91"/>
<pinref part="K7" gate="G$1" pin="2"/>
</segment>
</net>
<net name="SIG$65" class="0">
<segment>
<wire x1="-89.535" y1="43.815" x2="-90.805" y2="43.815" width="0.3" layer="91"/>
<pinref part="K5" gate="G$1" pin="4"/>
<wire x1="-90.805" y1="43.815" x2="-90.805" y2="29.21" width="0.3" layer="91"/>
<wire x1="-90.805" y1="29.21" x2="-76.835" y2="29.21" width="0.3" layer="91"/>
<wire x1="-76.835" y1="29.21" x2="-76.835" y2="40.64" width="0.3" layer="91"/>
<wire x1="-76.835" y1="40.64" x2="-51.435" y2="40.64" width="0.3" layer="91"/>
<pinref part="K7" gate="G$1" pin="3"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>
