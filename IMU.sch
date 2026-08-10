EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 2 4
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Wire Wire Line
	5950 3150 5850 3150
Wire Wire Line
	5850 3150 5850 3050
Wire Wire Line
	5850 3050 5950 3050
Wire Wire Line
	6450 3250 6550 3250
Wire Wire Line
	6550 3250 6550 3350
Wire Wire Line
	6550 3350 6450 3350
$Comp
L power:+3.3V #PWR018
U 1 1 61CFCD24
P 7200 3300
F 0 "#PWR018" H 7200 3150 50  0001 C CNN
F 1 "+3.3V" H 7215 3473 50  0000 C CNN
F 2 "" H 7200 3300 50  0001 C CNN
F 3 "" H 7200 3300 50  0001 C CNN
	1    7200 3300
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR015
U 1 1 61CFCF54
P 5150 3250
F 0 "#PWR015" H 5150 3100 50  0001 C CNN
F 1 "+3.3V" H 5165 3423 50  0000 C CNN
F 2 "" H 5150 3250 50  0001 C CNN
F 3 "" H 5150 3250 50  0001 C CNN
	1    5150 3250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR017
U 1 1 61CFD1BB
P 6950 3150
F 0 "#PWR017" H 6950 2900 50  0001 C CNN
F 1 "GND" H 6955 2977 50  0000 C CNN
F 2 "" H 6950 3150 50  0001 C CNN
F 3 "" H 6950 3150 50  0001 C CNN
	1    6950 3150
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR016
U 1 1 61CFDAF4
P 5450 3050
F 0 "#PWR016" H 5450 2800 50  0001 C CNN
F 1 "GND" H 5455 2877 50  0000 C CNN
F 2 "" H 5450 3050 50  0001 C CNN
F 3 "" H 5450 3050 50  0001 C CNN
	1    5450 3050
	1    0    0    -1  
$EndComp
Connection ~ 5850 3050
Text HLabel 5850 2950 0    50   Output ~ 0
uart2_tx_out_gps
Text HLabel 5850 2850 0    50   Input ~ 0
uart2_rx_in_gps
Text HLabel 5850 3650 0    50   Output ~ 0
uart_tx_out_imu
Wire Wire Line
	5850 3650 5950 3650
NoConn ~ 5950 3350
NoConn ~ 5950 3450
NoConn ~ 5950 3750
NoConn ~ 6450 3750
Wire Wire Line
	6450 3150 6950 3150
Wire Wire Line
	6550 3350 7200 3350
Wire Wire Line
	7200 3350 7200 3300
Connection ~ 6550 3350
Text HLabel 6550 3650 2    50   Input ~ 0
uart_rx_in_imu
Wire Wire Line
	5850 2850 5950 2850
Wire Wire Line
	5850 2950 5950 2950
Wire Wire Line
	6450 3650 6550 3650
Wire Wire Line
	5150 3250 5950 3250
$Comp
L Connector_Generic:Conn_01x04 J?
U 1 1 61E760E5
P 8700 3350
AR Path="/61E760E5" Ref="J?"  Part="1" 
AR Path="/61C583AF/61E760E5" Ref="JTAG_IMU1"  Part="1" 
F 0 "JTAG_IMU1" H 8650 3650 50  0000 L CNN
F 1 "Conn_01x04" H 8300 3000 50  0000 L CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical" H 8700 3350 50  0001 C CNN
F 3 "~" H 8700 3350 50  0001 C CNN
	1    8700 3350
	1    0    0    -1  
$EndComp
Text HLabel 8350 3350 0    50   Input ~ 0
swdio_imu
Text HLabel 8350 3450 0    50   Input ~ 0
swclk_imu
Wire Wire Line
	8350 3350 8500 3350
Wire Wire Line
	8500 3450 8350 3450
Text HLabel 8350 3250 0    50   Input ~ 0
nrst_imu
Wire Wire Line
	8350 3250 8500 3250
Text HLabel 6550 3050 2    50   Output ~ 0
swdio_imu
Text HLabel 6550 2950 2    50   Output ~ 0
swclk_imu
Text HLabel 6550 3450 2    50   Output ~ 0
nrst_imu
Wire Wire Line
	6550 3450 6450 3450
$Comp
L Connector_Generic:Conn_02x10_Odd_Even J5
U 1 1 61CEBCB3
P 6150 3350
F 0 "J5" H 6200 3967 50  0000 C CNN
F 1 "Conn_02x10_Odd_Even" H 6200 3876 50  0000 C CNN
F 2 "imu_connector_female_2x10:imu_connector_female_2x10" H 6150 3350 50  0001 C CNN
F 3 "~" H 6150 3350 50  0001 C CNN
	1    6150 3350
	1    0    0    1   
$EndComp
NoConn ~ 6450 2850
Wire Wire Line
	6550 2950 6450 2950
Wire Wire Line
	5450 3050 5850 3050
Wire Wire Line
	6450 3050 6550 3050
NoConn ~ 6450 3550
NoConn ~ 5950 3550
$EndSCHEMATC
