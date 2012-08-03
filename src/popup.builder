<?xml version="1.0"?>
<interface>
<object class="GtkMenu" id="menu">
	<child><object class="GtkImageMenuItem" id="toggle">
		<property name="label">Toggle</property>
		<signal name="activate" handler="popup" />
	</object></child>
	<child><object class="GtkSeparatorMenuItem" id="separator1" /></child>
	<child><object class="GtkImageMenuItem" id="preferences">
		<property name="use-stock">true</property>
		<property name="label">gtk-preferences</property>
		<signal name="activate" handler="popup" />
	</object></child>
	<child><object class="GtkImageMenuItem" id="about">
		<property name="use-stock">true</property>
		<property name="label">gtk-about</property>
		<signal name="activate" handler="popup" />
	</object></child>
	<child><object class="GtkSeparatorMenuItem" id="separator2" /></child>
	<child><object class="GtkImageMenuItem" id="quit">
		<property name="use-stock">true</property>
		<property name="label">gtk-quit</property>
		<signal name="activate" handler="popup" />
	</object></child>
</object>
</interface>
