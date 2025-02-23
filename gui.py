import PySimpleGUI as sg

import os.path
import subprocess
from PIL import Image, ImageTk


FILE_SELECTED = None

# First the window layout in 2 columns
file_list_column = [
    [
        sg.Text("Image Folder"),
        sg.In(size=(25, 2), enable_events=True, key="-FOLDER-"),
        sg.FolderBrowse(),
        sg.Button('Run Plate Recognition')
    ],
    [
        sg.Listbox(
            values=[], enable_events=True, size=(40, 20), key="-FILE LIST-"
        )
    ],

]

# For now will only show the name of the file that was chosen
image_viewer_column = [
    [sg.Text("Choose an image from list on left:")],
    [sg.Text(size=(40, 1), key="-TOUT-")],
    [sg.Image(key="-IMAGE-")],
]


# ----- Full layout -----
layout = [
    [
        sg.Column(file_list_column),
        sg.VSeperator(),
        sg.Column(image_viewer_column),
    ]
]


window = sg.Window("Image Viewer", layout)

# Run the Event Loop

while True:

    event, values = window.read()
    if event == "Exit" or event == sg.WIN_CLOSED:
        break

    # Folder name was filled in, make a list of files in the folder
    if event == "-FOLDER-":
        folder = values["-FOLDER-"]
        try:
            # Get list of files in folder
            file_list = os.listdir(folder)
        except:
            file_list = []

        fnames = [
            f
            for f in file_list
            if os.path.isfile(os.path.join(folder, f))
            and f.lower().endswith((".png", ".gif", ".jpg", ".jpeg", ".mp4"))
        ]
        window["-FILE LIST-"].update(fnames)

    elif event == "-FILE LIST-":  # A file was chosen from the listbox
        try:
            filename = os.path.join(
                values["-FOLDER-"], values["-FILE LIST-"][0]
            )
            FILE_SELECTED = filename

            im = Image.open(filename)
            image = ImageTk.PhotoImage(image=im)

            window["-TOUT-"].update(filename)
            window["-IMAGE-"].update(data=image)
        except:
            pass

    elif event == 'Run Plate Recognition':  # Extract button pressed
        try:
            print(FILE_SELECTED)
            name = FILE_SELECTED.split("/")
            
            if (FILE_SELECTED.lower().endswith((".png", ".gif", ".jpg", ".jpeg"))):
                result = subprocess.check_output(['./a.out', name[7], '1'])
            elif (FILE_SELECTED.lower().endswith((".mp4"))):
                result = subprocess.check_output(['./a.out', name[7], '2'])
            
            print(result)
            result = result.decode("utf-8").replace("\n", "")

            sg.popup('Plate Recognition Result:', result)
        except:
            pass

window.close()