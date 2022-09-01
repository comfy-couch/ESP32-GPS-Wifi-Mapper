# ESP32 GPS/Wifi Mapper
 Tool to roughly map Wifi APs using GPS and RSSI data

This project is not complete, it is missing a huge chunk of required math and desktop integration still! For now this repo will only focus on the hardware and code driving it.

Have you ever lost your router? *Me neither*, but that should never stop us from building new, questionably useful tools and toys. This project aims to connect GPS and Wifi network signal strengths (RSSI) to roughly map out the location of broadcasting stations. In theory, using the ESP32 will also allow you to do the same for Bluetooth devices but for now this project only deals with Wifi.

By using a number of gracefully provided libraries we can get a gizmo together that can be handheld in a pocket or on a car dashboard to run a site survey of your neighborhood to find out which network is from which house. Or at least that's the eventual goal, I'll get into the next steps needed and expected issues near the bottom of this document. Before that, let's look at what does work and part of how.

## First, components.
- The workhorse here is an [Adafruit HUZZAH32 ESP32 Feather](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/overview) board. It gives us plenty of memory and program space to hold everything but more importantly comes with Wifi and Bluetooth built in and enough power to drive the peripherals I had on hand when putting this together.
- The GPS unit in use is a cheap (~$13USD) GT-U7 module from Amazon, there are plenty of sellers for this particular device and I'm willing to bet they're all as finicky as the one I got. This would be the first thing I'd personally replace/upgrade in this project but for now it's fine. You'll see a little hot glue keeping the antenna connector right where it's happy; a major annoyance on this module.
- The display in use here is an SSD1331 compatible *color* OLED screen. This project started with a monochrome but the pixel aspect ratio and limitation  of a single color made it difficult to quickly read and limited the "Ooooh *neat*" aspect of showing it off. The tradeoff is that once again I used a cheap unit (~$13USD) which doesn't give the best performance. It is 0.95 inches with a resolution of 96x64 and connects over SPI. Thankfully we have a great library that helps improve performance on this little dude.
- I had a battery sitting around so it's part of this project. It would work just as happily powered via USB and there is a definition in the code that can be commented out to completely remove all the battery monitoring and display. In this case it's an [Adafruit 3.7v 1200mAh](https://www.adafruit.com/product/258) cell. The code references some values for full and empty that were determined by charging the cell and fully draining it, something you'd likely need to do with whatever battery you want to use. It also uses some very ugly linear math to guess at remaining power to save from doing more complex math but is generally not unreliable.
- Misc hardware includes the double sided PCB prototyping boards, a few tactile buttons, a simple rocker switch, flexible silicone wire and (naturally) hot glue. The buttons are for interacting with the system so you'll need a few of those in your favorite flavor. This could be (and was) done on a breadboard so if you don't want to get PCBs, don't. They just make it look a little nicer (arguably). The rocker switch is mostly if you want to use battery as it connects the ENable pin to ground to disable the ESP32 when not in use while still allowing the battery to charge. Without a battery you could easily skip it and just unplug the thing.

## Now, libraries.
**[TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)**

This library makes it very easy to read in the data from the GPS and parse it out into useful bits for our uses. This code also uses a number of the functions provided in their examples for printing data to the serial monitor since they do a nice job of it. The only change made is replacing `sprintf` with `snprintf` to remove potential overflows anywhere.

**[Arduino GFX Library](https://github.com/moononournation/Arduino_GFX)**

This gem takes the place of the Adafruit GFX library and allows much much faster SPI communication with the OLED. In some benchmarks the speedup is in the hundreds of percent faster (16fps vs 180fps). By allowing us to define the Hardware SPI interface on the ESP32 we get much better performance out of the hardware. Instead of a visible rolling update, there's a slight blink when we redraw the display.

**[ezButton](https://arduinogetstarted.com/tutorials/arduino-button-library])**

Eventually I may come back and implement my own button handling but to get things going this library provides good monitoring and automatic debouncing on our switches.

**[LinkedList](https://github.com/ivanseidel/)**

What a wonderful person to save me from having to implement my own Linked List system. This code uses two LLs to keep track of locations and networks seen at each location, then references them together with a simple ID. Is this the most efficient  way to do this? Probably not, but it *does* work.

## Theory of Operation
At power on the GPS unit will receive  power and automatically begin searching for satellites. The module I used has a teeny-tiny battery onboard and can only keep time and AGPS data for a matter of minutes meaning that it sometimes takes 5+ minutes to get a fix. Something with a more robust RTC system will keep time and fixes much longer and speed subsequent boots. The rest of the system is ready in just a moment as there's not much to do during setup. The display will show how many satellites are locked and the current location and become green when a fix is reasonably good (in testing, 4 satellites is enough to be within 50M, good enough for early testing).
When the user has an acceptable number of satellites locked (up to you, there's nothing stopping you from working with no satellites) they can hit the "SCAN" button. This causes the ESP32 to find nearby 2.4GHz networks which it automatically lists in order of signal strength. The display shows when scanning is complete and lists the number of networks seen. This process can and should be repeated several times at different spots around the area of interest to gather more data.
Once a scan has been saved, the user can press the "DISP" button to be shown a color-coded list of networks seen during the last scan. More green is stronger, down to dim red for weak signals.
Once connected to a computer and the serial monitor is opened (or a utility like PuTTY on a COM port), the "DUMP" button will print every location and the networks seen there along with signal strength observed. Parsing this data can give an idea of signal origination through triangulation. This part of the project is not ready (or started, actually) as I need time to perform data collection from known distances and through different materials/houses. For now it's a lot of guessing and hoping.

## Construction
Here we'll talk about the jank that is actually wiring all this together. First up is just what my build looks like. It's using headers rather than soldering the modules directly to the PCB to allow some flexibility in revising it later at the cost of extra bulk and some additional  ugliness:

![overview](/img/overview.png)

Here we see the ESP32, GPS, buttons, and OLED display in standby. The GPS is receiving data and the system is updating the display every second. It is ready to scan for wifi, or as a scan is already saved, dump/display that result.

### Wiring
These images give a rough overlay to how things are wired up. They're not the neatest in layout or design but should give a good enough idea of what's hooked to where.

With the modules pulled from the headers, it exposes the wiring for the OLED to the ESP. The wiring for the GPS and buttons are on the underside of the PCB to keep clutter down. The rocker switch connects the ENable pin to ground on the underside as well.

![disassembled](/img/disassembled.png)

The GPS:

![gps](/img/gps.png)

Buttons:

![buttons](/img/buttons.png)

The OLED display:

![coled](/img/coled.png)

## Project Status and limitations
- The data collection part is mostly done here; the unit pulls data and networks, linking them together for later data retrieval provided it keeps power on. I have not begun storing data through power cycles to limit writing to the module's flash for now. The addition of an SD card reader or using a module that contains one would be best for storing data, as well as possibly eliminating the need to use serial to retrieve data.
- The mapping portion is still not even properly begun, data is needed for roughly ranging an AP based on this device's antenna and the broadcasting station. This will likely take some independent research of my own routers and known locations until some models can be generated. A standardized output that can be imported into an app like Google Earth would also help ease mapping.
- The ESP32, while powerful, still doesn't work with 5GHz networks. These will be invisible with this hardware.
- Hidden networks are currently not being mapped, although eventually I may add them in by BSSID (MAC address).
- Eventually the hardware could also be used for Bluetooth devices in certain states but this is a future addition that will be explored after getting mapping working at least roughly.