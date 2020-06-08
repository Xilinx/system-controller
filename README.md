# system-controller

	Usage:

	sc_app -c <command> [-t <target> [-v <value>]]

	<command> - 
		version - version and compatibility information
		listbootmode - lists the supported boot mode targets
		setbootmode - set boot mode to <target>
		reset - apply power-on-reset
		eeprom - lists the content of EEPROM
		listclock - lists the supported clock targets
		getclock - get the frequency of <target>
		setclock - set <target> to <value> frequency
		restoreclock - restore <target> to default value
		listvoltage - lists the supported voltage targets
		getvoltage - get the voltage of <target>
		listpower - lists the supported power targets
		getpower - get the voltage, current, and power of <target>
		listworkaround - lists the applicable workaround targets
		workaround - apply <target> workaround (may requires <value>)
		listBIT - lists the supported Board Interface Test targets
		BIT - run BIT target
		ddr - get DDR DIMM information: <target> is either 'spd' or 'temp'

