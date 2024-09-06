const MIDIMAP_VELOCITY = 0;
const MIDIMAP_CC = 1;
const MIDIMAP_PITCH = 2;
const MIDIMAP_PITCH_SAH = 3;
const MIDIMAP_RANDSEQ = 4;
const MIDIMAP_RANDSEQ_SAH = 5;

const channelOptions = Array.from({ length: 16 }, (_, i) => ({ value: 0x90 + i, text: `Channel ${i + 1}` }));
const noteOptions = Array.from({ length: 128 }, (_, i) => ({ value: i, text: `Note ${i}` }));
const controllerOptions = Array.from({ length: 128 }, (_, i) => ({ value: i, text: `Controller ${i}` }));

const midiModeOptions = [
    {
        value: 0,
        text: "Velocity",
        requiredOptions: [channelOptions, noteOptions],
        requiredLabels: ["Gate Channel", "Gate Note"]
    },
    {
        value: 1,
        text: "Control Change",
        requiredOptions: [channelOptions, noteOptions, channelOptions, controllerOptions],
        requiredLabels: ["Gate Channel", "Gate Note", "Controller Channel", "Controller"]
    },
    {
        value: 2,
        text: "Pitch",
        requiredOptions: [channelOptions],
        requiredLabels: ["Pitch Channel"]
    },
    {
        value: 3,
        text: "Pitch, Sample & Hold",
        requiredOptions: [channelOptions, noteOptions, channelOptions],
        requiredLabels: ["Gate Channel", "Gate Note", "Pitch Channel"]
    },
    {
        value: 4,
        text: "Random Step Sequencer",
        requiredOptions: [channelOptions, noteOptions, channelOptions, noteOptions, channelOptions, noteOptions],
        requiredLabels: ["Gate Channel", "Gate Note", "Step Channel", "Step Note", "Reset Channel", "Reset Note"]
    },
    {
        value: 5,
        text: "Random Step Sequencer, Sample & Hold",
        requiredOptions: [channelOptions, noteOptions, channelOptions, noteOptions, channelOptions, noteOptions],
        requiredLabels: ["Gate Channel", "Gate Note", "Step Channel", "Step Note", "Reset Channel", "Reset Note"]
    }
];

function loadInitialData() {
    const savedArray = localStorage.getItem('globalArray');
    if (savedArray) {
        return JSON.parse(savedArray);
    } else {
        return [
            [MIDIMAP_VELOCITY, 0x90, 24, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 25, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 26, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 27, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 28, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 29, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 30, 0, 0, 0, 0],
            [MIDIMAP_VELOCITY, 0x90, 31, 0, 0, 0, 0]
        ];
    }
}

function initializeDropdowns() {
    const dropdownArea = document.getElementById('dropdownArea');
    dropdownArea.innerHTML = '';
    globalArray = loadInitialData();
    console.log("Loaded array: ", globalArray);

    globalArray.slice(0, 8).forEach((row, index) => {
        const selectedMidiMode = row[0];
        const dropdown = createDropdown(midiModeOptions, selectedMidiMode, (newValue) => {
            updateGlobalArray(index, 0, newValue);
            rebuildChildDropdowns(index, newValue);
            updateTextAreaFromArray();
        });

        const label = document.createElement('label');
        label.textContent = `${index + 1}.`;

        const labelDropdownWrapper = document.createElement('div');
        labelDropdownWrapper.classList.add('label-dropdown-wrapper');
        labelDropdownWrapper.appendChild(label);
        labelDropdownWrapper.appendChild(dropdown);

        const container = document.createElement('div');
        container.classList.add('dropdown-container');
        container.appendChild(labelDropdownWrapper);

        const childContainer = document.createElement('div');
        childContainer.classList.add('child-container');
        container.appendChild(childContainer);

        dropdownArea.appendChild(container);

        rebuildChildDropdowns(index, selectedMidiMode);
    });

    updateTextAreaFromArray();
}

function rebuildChildDropdowns(rowIndex, midiMode) {
    const container = document.querySelectorAll('.child-container')[rowIndex];
    container.innerHTML = ''; // Clear existing dropdowns

    const row = globalArray[rowIndex];
    const { requiredLabels, requiredOptions } = midiModeOptions[midiMode];

    requiredLabels.forEach((labelText, i) => {
        const dropdownOptions = requiredOptions[i];
        const selectedValue = row[i + 1];

        const dropdown = createDropdown(dropdownOptions, selectedValue, (newValue) => {
            updateGlobalArray(rowIndex, i + 1, newValue);
            updateTextAreaFromArray();
        });

        const label = document.createElement('label');
        label.textContent = labelText;

        const wrapper = document.createElement('div');
        wrapper.classList.add('label-dropdown-wrapper');
        wrapper.appendChild(label);
        wrapper.appendChild(dropdown);

        container.appendChild(wrapper);
    });
}


function createDropdown(options, selectedValue, onChangeCallback) {
    const dropdown = document.createElement('select');
    options.forEach(option => {
        const opt = document.createElement('option');
        opt.value = option.value;
        opt.textContent = option.text;
        if (option.value === selectedValue) {
            opt.selected = true;
        }
        dropdown.appendChild(opt);
    });

    dropdown.addEventListener('change', function (event) {
        const newValue = parseInt(event.target.value);
        onChangeCallback(newValue);
    });

    return dropdown;
}

function updateGlobalArray(rowIndex, columnIndex, newValue) {
    globalArray[rowIndex][columnIndex] = newValue;
    console.log("Updated globalArray:", globalArray);
    localStorage.setItem('globalArray', JSON.stringify(globalArray));
}

function updateTextAreaFromArray() {
    const arrayTextArea = document.getElementById('arrayTextArea');

    const maskedArray = globalArray.slice(0, 8).map((row) => {
        const numKeep = midiModeOptions[row[0]].requiredLabels.length + 1;
        const numPad = row.length - numKeep; 
        return row.slice(0, numKeep).concat(Array(numPad).fill(0)); 
    });

    arrayTextArea.value = JSON.stringify(maskedArray, null, 0);
}

function updateArrayFromTextArea() {
    console.log("Tryinng to update from textbox...");
    const arrayTextArea = document.getElementById('arrayTextArea');

    try {
        const parsedArray = JSON.parse(arrayTextArea.value);

        if (Array.isArray(parsedArray)) {
            parsedArray.slice(0, 8).forEach((row, index) => {
                const numKeep = midiModeOptions[row[0]].requiredLabels.length + 1
                globalArray[index].splice(0, numKeep, ...parsedArray[index].slice(0, numKeep));
            });

            localStorage.setItem('globalArray', JSON.stringify(globalArray));

            console.log("globalArray updated successfully:", globalArray);
            initializeDropdowns();
        } else {
            console.error("The content in the textarea is not a valid array.");
        }
    } catch (error) {
        console.error("Invalid JSON format:", error);
    }
}

async function sendSysExMessageWithPauses() {
    if (navigator.requestMIDIAccess) {
        try {
            const midiAccess = await navigator.requestMIDIAccess({ sysex: true });
            onMIDISuccess(midiAccess);
        } catch (error) {
            console.error("Failed to access MIDI devices:", error);
        }
    } else {
        alert("Web MIDI API is not supported in this browser.");
    }
}

async function onMIDISuccess(midiAccess) {
    const outputs = Array.from(midiAccess.outputs.values());

    const maskedArray = globalArray.slice(0, 8).map((row) => {
        const numKeep = midiModeOptions[row[0]].requiredLabels.length + 1;
        const numPad = row.length - numKeep; 
        return row.slice(0, numKeep).concat(Array(numPad).fill(0)); 
    });

    if (outputs.length > 0) {
        const output = outputs[0];
        const sysExMessage = asSysEx(maskedArray)
        output.send(sysExMessage);
        console.log("Sent SysEx message:", sysExMessage);
    } else {
        console.log("No MIDI output devices available.");
    }
}

function asSysEx(array) {
    const sysExArray = [0xF0];
    array.flat().forEach(value => {
        sysExArray.push(value & 0x7F);  // Push the least significant 7 bits
        sysExArray.push((value >>= 7) & 0x7F);  // Push the remaining 7 bits if non-zero
    });
    sysExArray.push(0xF7);
    return sysExArray;
}

function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

document.getElementById("sendSysExButton").addEventListener("click", sendSysExMessageWithPauses);
window.onload = initializeDropdowns;
