<?xml version="1.0"?>
<interface>

<object class="GtkAdjustment" id="keywidth">
	<property name="lower">1</property>
	<property name="upper">4096</property>
	<property name="step-increment">1</property>
</object>
<object class="GtkAdjustment" id="keyheight">
	<property name="lower">1</property>
	<property name="upper">4096</property>
	<property name="step-increment">1</property>
</object>
<object class="GtkAdjustment" id="xpadding">
	<property name="lower">0</property>
	<property name="upper">1</property>
	<property name="step-increment">0.01</property>
</object>
<object class="GtkAdjustment" id="ypadding">
	<property name="lower">0</property>
	<property name="upper">1</property>
	<property name="step-increment">0.01</property>
</object>
<object class="GtkAdjustment" id="maxheight">
	<property name="lower">0</property>
	<property name="upper">1</property>
	<property name="step-increment">0.1</property>
</object>
<object class="GtkAdjustment" id="fontsize">
	<property name="lower">0</property>
	<property name="upper">4096</property>
	<property name="step-increment">0.1</property>
</object>

<object class="GtkDialog" id="window">
	<property name="title">GBoard Preferences</property>
	<property name="resizable">false</property>
	<child internal-child="vbox"><object class="GtkBox" id="testvbox">
		<property name="border-width">10</property>
		<property name="homogeneous">false</property>
		<property name="spacing">4</property>
		<child><object class="GtkLabel" id="lbl_layout">
				<property name="label">Default Layout</property>
		</object></child>
		<child><object class="GtkFileChooserButton" id="layout">
			<property name="title">Default Layout</property>
		</object></child>
		<child><object class="GtkCheckButton" id="reset">
				<property name="label">Reset Dockstate</property>
		</object></child>
		<child><object class="GtkCheckButton" id="north">
			<property name="label">North</property>
		</object></child>
		<child><object class="GtkLabel" id="lbl_padding">
			<property name="label">Key Padding</property>
		</object></child>
		<child><object class="GtkBox" id="box_padding">
			<child><object class="GtkSpinButton" id="spin_xpadding">
				<property name="digits">3</property>
				<property name="adjustment">xpadding</property>
			</object></child>
			<child><object class="GtkLabel" id="lbl_paddingx">
				<property name="hexpand">true</property>
				<property name="label"> x </property>
			</object></child>
			<child><object class="GtkSpinButton" id="spin_ypadding">
				<property name="digits">3</property>
				<property name="adjustment">ypadding</property>
			</object></child>
		</object></child>
		<child><object class="GtkLabel" id="lbl_size">
			<property name="label">Key Size</property>
		</object></child>
		<child><object class="GtkBox" id="box_size">
			<child><object class="GtkSpinButton" id="spin_xsize">
				<property name="adjustment">keywidth</property>
			</object></child>
			<child><object class="GtkLabel" id="lbl_sizex">
				<property name="hexpand">true</property>
				<property name="label"> x </property>
			</object></child>
			<child><object class="GtkSpinButton" id="spin_ysize">
				<property name="adjustment">keyheight</property>
			</object></child>
		</object></child>
		<child><object class="GtkBox" id="box_maxheight">
			<child><object class="GtkLabel" id="lbl_maxheight">
				<property name="hexpand">true</property>
				<property name="halign">GTK_ALIGN_START</property>
				<property name="label">Max. Height</property>
			</object></child>
			<child><object class="GtkSpinButton" id="spin_maxheight">
				<property name="digits">3</property>
				<property name="adjustment">maxheight</property>
			</object></child>
		</object></child>
		<child><object class="GtkBox" id="box_fontsize">
			<child><object class="GtkLabel" id="lbl_fontsize">
				<property name="hexpand">true</property>
				<property name="halign">GTK_ALIGN_START</property>
				<property name="label">Fontsize</property>
			</object></child>
			<child><object class="GtkSpinButton" id="spin_fontsize">
				<property name="digits">3</property>
				<property name="adjustment">fontsize</property>
			</object></child>
		</object></child>
		<child><object class="GtkCheckButton" id="relative">
			<property name="label">Fontsize relative fraction</property>
		</object></child>
		<child><object class="GtkCheckButton" id="leftmenu">
			<property name="label">Menu on Systray-Left-Click</property>
		</object></child>
		<child><object class="GtkCheckButton" id="shaped">
			<property name="label">Window Shaping</property>
		</object></child>
		<child><object class="GtkCheckButton" id="hardhide">
			<property name="label">Forced Window Hiding</property>
		</object></child>
	</object></child>
	<child internal-child="action_area"><object class="GtkButtonBox" id="testbox">
		<child><object class="GtkButton" id="cancel">
			<property name="label">gtk-cancel</property>
			<property name="use-stock">true</property>
		</object></child>
		<child><object class="GtkButton" id="ok">
			<property name="label">gtk-ok</property>
			<property name="use-stock">true</property>
		</object></child>
	</object></child>
</object></interface>
