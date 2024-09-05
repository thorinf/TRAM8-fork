const MIDIMAP_VELOCITY = 0;

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

const channelOptions = Array.from({ length: 16 }, (_, i) => ({ value: 0x90 + i, text: `Channel ${i + 1}` }));
const noteOptions = Array.from({ length: 128 }, (_, i) => ({ value: i, text: `Note ${i}` }));
const controllerOptions = Array.from({ length: 128 }, (_, i) => ({ value: i, text: `Controller ${i}` }));

const midiModeOptions = [
    { 
        value: 0, 
        text: "Velocity", 
        visibleCount: 3, 
        requiredOptions: [channelOptions, noteOptions], 
        requiredLabels: ["Gate Channel", "Gate Note"] 
    },
    { 
        value: 1, 
        text: "Control Change", 
        visibleCount: 5, 
        requiredOptions: [channelOptions, noteOptions, channelOptions, controllerOptions], 
        requiredLabels: ["Gate Channel", "Gate Note", "Controller Channel", "Controller"] },
    { 
        value: 2, 
        text: "Pitch", 
        visibleCount: 2, 
        requiredOptions: [channelOptions], 
        requiredLabels: ["Pitch Channel"] },
    { 
        value: 3, 
        text: "Pitch, Sample & Hold", 
        visibleCount: 4, 
        requiredOptions: [channelOptions, noteOptions, channelOptions], 
        requiredLabels:  ["Gate Channel", "Gate Note", "Pitch Channel"] },
    { 
        value: 4, 
        text: "Random Step Sequencer", 
        visibleCount: 7, 
        requiredOptions: [channelOptions, noteOptions, channelOptions, noteOptions, channelOptions, noteOptions], 
        requiredLabels:  ["Gate Channel", "Gate Note", "Step Channel", "Step Note", "Reset Channel", "Reset Note"] },
    { 
        value: 5, 
        text: "Random Step Sequencer, Sample & Hold", 
        visibleCount: 7, 
        requiredOptions: [channelOptions, noteOptions, channelOptions, noteOptions, channelOptions, noteOptions], 
        requiredLabels:  ["Gate Channel", "Gate Note", "Step Channel", "Step Note", "Reset Channel", "Reset Note"] 
    }
];

function initializeDropdowns() {
    const dropdownArea = document.getElementById('dropdownArea');
    dropdownArea.innerHTML = '';
    globalArray = loadInitialData();
    globalArray.forEach((row, rowIndex) => {
        const rowDiv = document.createElement('div');
        rowDiv.className = 'dropdownRow';
        rowDiv.id = `row-${rowIndex}`;

        row.forEach((value, colIndex) => {
            if (colIndex > 0) {
                const label = document.createElement('label');
                label.htmlFor = `dropdown-${rowIndex}-${colIndex}`;
                label.textContent = midiModeOptions[globalArray[rowIndex][0]].requiredLabels[colIndex - 1] || "";
                rowDiv.appendChild(label);
            }

            const select = document.createElement('select');
            select.id = `dropdown-${rowIndex}-${colIndex}`;
            select.onchange = () => {
                let newValue = colIndex === 0 ? parseInt(select.value, 10) : select.value;
                updateArrayFromDropdown(rowIndex, colIndex, newValue);
                updateVisibility(rowIndex);
                updateDisplay();
            };
            populateDropdownOptions(select, colIndex === 0 ? midiModeOptions : midiModeOptions[globalArray[rowIndex][0]].requiredOptions[colIndex - 1] || [], value);
            rowDiv.appendChild(select);
        });

        dropdownArea.appendChild(rowDiv);
    });
    updateVisibilityForAllRows(); 
    updateDisplay(); 
}

function updateVisibilityForAllRows() {
    globalArray.forEach((row, rowIndex) => {
        updateVisibility(rowIndex);
    });
}

function updateVisibility(rowIndex) {
    const row = document.getElementById(`row-${rowIndex}`);
    const selects = row.getElementsByTagName('select');
    const modeOption = midiModeOptions[globalArray[rowIndex][0]];
    const visibleCount = modeOption.visibleCount;

    for (let i = 1; i < selects.length; i++) {
        const label = selects[i].previousElementSibling;
        if (i < visibleCount) {
            selects[i].style.display = '';
            if (label) label.style.display = '';
            populateDropdownOptions(selects[i], modeOption.requiredOptions[i - 1], globalArray[rowIndex][i]);
            label.textContent = modeOption.requiredLabels[i - 1] || "";
        } else {
            // Hide and reset hidden elements
            selects[i].style.display = 'none';
            globalArray[rowIndex][i] = 0; // Reset to 0
            if (label) label.style.display = 'none';
        }
    }
}

function populateDropdownOptions(dropdown, options, selectedValue) {
    dropdown.innerHTML = '';
    options.forEach(option => {
        const optionElement = new Option(option.text, option.value);
        optionElement.selected = option.value === parseInt(selectedValue, 10);
        dropdown.appendChild(optionElement);
    });
}

function updateArrayFromDropdown(rowIndex, colIndex, newValue) {
    if (colIndex === 0) {
        const oldMode = midiModeOptions[globalArray[rowIndex][0]];
        const newMode = midiModeOptions[newValue];

        if (newMode !== oldMode) {
            resetRowValues(rowIndex, newValue, oldMode);
        }
    } else {
        globalArray[rowIndex][colIndex] = parseInt(newValue, 10);
    }

    localStorage.setItem('globalArray', JSON.stringify(globalArray));
    console.log(`Updated array at row ${rowIndex}:`, globalArray[rowIndex]);
}

function resetRowValues(rowIndex, newModeIndex, oldMode) {
    const newMode = midiModeOptions[newModeIndex];
    let newRowValues = [newModeIndex]; 

    for (let i = 1; i < globalArray[rowIndex].length; i++) {
        if (i < newMode.visibleCount) {
            const optionExistsInOldMode = oldMode.requiredOptions[i - 1] && oldMode.requiredOptions[i - 1].length > 0;
            const optionExistsInNewMode = newMode.requiredOptions[i - 1] && newMode.requiredOptions[i - 1].length > 0;

            if (optionExistsInOldMode && optionExistsInNewMode) {
                newRowValues.push(globalArray[rowIndex][i]);
            } else {
                newRowValues.push(newMode.requiredOptions[i - 1][0] ? newMode.requiredOptions[i - 1][0].value : 0);
            }
        } else {
            newRowValues.push(0); 
        }
    }
    globalArray[rowIndex] = newRowValues;
    updateVisibility(rowIndex);
}

function updateDisplay() {
    const arrayTextArea = document.getElementById('arrayTextArea');
    let displayText = globalArray.map(row => 
        '[' + row.map(item => String(item).padStart(3, ' ')).join(", ") + ']'
    ).join("\n");

    arrayTextArea.value = displayText;
}

function updateFromText() {
    const arrayTextArea = document.getElementById('arrayTextArea');
    try {
        const newArray = arrayTextArea.value.split("\n").map(row => 
            row.replace(/[\[\]]/g, '')
               .split(", ")            
               .map(Number)            
        );

        if (Array.isArray(newArray) && newArray.length === globalArray.length &&
            newArray.every(row => row.length === globalArray[0].length)) {
            globalArray = newArray;
            globalArray.forEach((row, rowIndex) => {
                row.forEach((value, colIndex) => {
                    const selectId = `dropdown-${rowIndex}-${colIndex}`;
                    const select = document.getElementById(selectId);
                    if (select) {
                        select.value = value;
                        updateVisibility(rowIndex);
                    }
                });
            });
            localStorage.setItem('globalArray', JSON.stringify(globalArray));
            updateVisibilityForAllRows();
        } else {
            console.error("Invalid array format or dimensions.");
        }
    } catch (e) {
        console.error("Error parsing array from text:", e);
    }
}

window.onload = initializeDropdowns;
