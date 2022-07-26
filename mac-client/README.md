# RTM MacOS Client

### Requirements

- Xcode 12+

- MacOS 11.0+

### How to run

1. Clone the repo (or download the ZIP file) and then double click on the `RTMC.xcodeproj` file to open the project.

    a. Make sure to click `Install` Xcode components if prompted. 

2. To run, press the "Play" button on the top left of the Xcode window or press `⌘ + R`.


### Creating a Release (.dmg file)

1. In Xcode, go to `Product -> Archive`.

2. Select the most recent Archive and then click `Distribute App`.

3. Select `Copy App` and then click Next.

4. Select where you'd like to export the directory and then click `Export`.

5. Now that we exported the `.app` file, open a new Terminal.

6. To create a `.dmg` from the exported `.app` file, run:

```bash
hdiutil create -srcfolder <YOUR_EXPORTED_APP_DIR> myNewdmg.dmg
``` 
