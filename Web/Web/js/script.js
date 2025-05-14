const toggle = document.querySelector(".toggle");
const add = document.querySelector(".add");
const main = document.querySelector(".content-main");
const inner = document.querySelector(".content-inner");
const back = document.querySelector(".back-button");
const AddFirstMed = document.getElementById("AddFirstMed");

function getCurrentTime() {
    const now = new Date();
    const hours = now.getHours();
    const minutes = now.getMinutes();
    const ampm = hours >= 12 ? 'PM' : 'AM';
    const hourIn12 = hours % 12 || 12;
    const formattedTime = `${hourIn12}:${minutes < 10 ? '0' + minutes : minutes} ${ampm}`;
    return formattedTime;
  }


toggle.addEventListener("click", function() {
    this.classList.toggle('active');
    main.classList.toggle('hide');
    inner.classList.toggle('show');
});

add.addEventListener("click", function() {
    $('.container').hide();
    $('.add-pill-container').show()
});

AddFirstMed.addEventListener("click", function() {
    $('.container').hide();
    $('.add-pill-container').show()
});

back.addEventListener("click", function() {
    $('.add-pill-container').hide()
    $('.container').show();
});


// Function to generate the current week's dates
function generateCurrentWeek() {
    const today = new Date();
    const dayOfWeek = today.getDay(); // 0 (Sunday) to 6 (Saturday)
    const startOfWeek = new Date(today); // Clone today's date
    startOfWeek.setDate(today.getDate() - dayOfWeek); // Adjust to Monday (start of the week)

    const calendar = document.getElementById('calendar');
    calendar.innerHTML = ''; // Clear the calendar div

    // Loop through the days of the week
    for (let i = 0; i < 7; i++) {
        const day = new Date(startOfWeek);
        day.setDate(startOfWeek.getDate() + i);

        // Create and append elements for each day
        const time = document.createElement('time');
        const abbr = document.createElement('abbr');
        const span = document.createElement('span');

        // Get day name and date
        const dayName = day.toLocaleString('en-us', { weekday: 'short' }); // Mon, Tue, etc.
        const dateNumber = day.getDate();

        abbr.textContent = dayName;
        span.textContent = dateNumber;

        time.appendChild(abbr);
        time.appendChild(span);
        calendar.appendChild(time);

        // Highlight the current day
        if (day.toDateString() === today.toDateString()) {
            time.classList.add('active');
        }
        const options = { day: '2-digit', month: 'short' }; 
        const formattedDate = today.toLocaleDateString('en-GB', options);

        // Update the HTML element with the formatted date
        const today_date = document.getElementById("today-date");
        today_date.innerHTML = 'Today, ' + formattedDate;
    }
}

// Call the function to populate the calendar
generateCurrentWeek();

// Your web app's Firebase configuration
const firebaseConfig = {
    apiKey: "AIzaSyDO0uzzxzqLgfenngsAIJbvV5riUilnSn8",
    authDomain: "abcd-21cfe.firebaseapp.com",
    databaseURL: "https://abcd-21cfe-default-rtdb.firebaseio.com",
    projectId: "abcd-21cfe",
    storageBucket: "abcd-21cfe.firebasestorage.app",
    messagingSenderId: "383825682733",
    appId: "1:383825682733:web:e4568a0b63a88bdc6033a4"
  };

// Initialize Firebase
const app = firebase.initializeApp(firebaseConfig);
const db = firebase.database().ref('db');

document.getElementById('amount').addEventListener('change', function() {
    const notificationContainer = document.getElementById('notificationContainer');
    notificationContainer.innerHTML = ''; // Clear existing notification fields

    const amount = parseInt(this.value);
    for (let i = 0; i < amount; i++) {
        const newNotification = document.createElement('div');
        newNotification.className = 'notification-group';
        newNotification.innerHTML = `
            <i class="fas fa-bell"></i>
            <input type="time" class="notificationTime" required>
        `;
        notificationContainer.appendChild(newNotification);
    }
});

document.querySelectorAll('.food-pills-group .option').forEach(option => {
    option.addEventListener('click', function() {
        document.querySelectorAll('.food-pills-group .option').forEach(opt => opt.classList.remove('selected'));
        this.classList.add('selected');
        this.querySelector('input').checked = true;
        document.getElementById('foodPills').value = this.getAttribute('value');
    });
});

document.querySelectorAll('.cabin-group .option').forEach(option => {
    option.addEventListener('click', function() {
        document.querySelectorAll('.cabin-group .option').forEach(opt => opt.classList.remove('selected'));
        this.classList.add('selected');
        this.querySelector('input').checked = true;
        document.getElementById('cabins').value = this.getAttribute('value');
    });
});



document.getElementById('addPlanForm').addEventListener('submit', function (e) {
    e.preventDefault();

    const notificationTimes = Array.from(document.querySelectorAll('.notificationTime')).map(input => input.value);

    // Capture the form data
    const planData = {
        pillsName: document.getElementById('pillsName').value,
        amount: document.getElementById('amount').value,
        scheduleType: document.getElementById('duration').value,
        foodPills: document.querySelector('input[name="foodPills"]:checked').value,
        container: document.querySelector('input[name="cabins"]:checked').value, // Selected container
        notifications: notificationTimes, // Dynamically added notification times
        lastDispensed: "2024-11-22T00:00:00Z"
    };

    const containerId = `container-${planData.container}`; // e.g., "container-1"

    db.child('users')
    .child('containers')
    .child(containerId)
    .set(planData)
    .then(() => {
        console.log('Plan added successfully!');
    })
    .catch((error) => {
        console.error('Error adding plan: ', error);
    });

    runUpcomingPillLogic();

    document.getElementById('addPlanForm').reset();
    $('.add-pill-container').hide()
    $('.container').show();

});

// Function to find the upcoming pills
let isUpdating = false; // Prevent multiple calls to avoid duplication

function runUpcomingPillLogic() {
  const pillContainer = document.getElementById('pill-container');
  pillContainer.innerHTML = ''; // Clear previous content

  // Fetch all containers data for a user from Firebase
  db.child('users')
      .child('containers')
      .get() // Get all containers
      .then((snapshot) => {
          if (snapshot.exists()) {
              const containersData = snapshot.val(); // Fetch all containers data

              for (const containerId in containersData) {
                  const containerData = containersData[containerId];
                  const { pillsName, notifications, scheduleType, lastDispensed } = containerData;

                  if (notifications && notifications.length > 0) {
                      // Filter and remove duplicates
                      const uniqueNotifications = [...new Set(notifications)];

                      // Get only notifications matching the schedule logic
                      const scheduledNotifications = filterBySchedule(
                          uniqueNotifications,
                          scheduleType,
                          lastDispensed
                      );

                      // Filter the upcoming pill times
                      const upcomingTimes = filterUpcomingTimes(scheduledNotifications);

                      if (upcomingTimes.length > 0) {
                          upcomingTimes.forEach((time) => {
                              const pillDiv = document.createElement('div');
                              pillDiv.className = 'review-item';
                              pillDiv.innerHTML = `
                                  <div class="icon">
                                      <i class="fas fa-capsules"></i>
                                  </div>
                                  <div class="details">
                                      <h3>${pillsName}</h3>
                                      <label>${time}</label>
                                  </div>
                                  <div class="arrow">
                                      <i class="fas fa-chevron-right"></i>
                                  </div>
                              `;
                              pillContainer.appendChild(pillDiv);
                          });
                      } else {
                          console.log(`No upcoming notifications for container ${containerId}.`);
                      }
                  } else {
                      console.log(`No notifications found for container ${containerId}.`);
                  }
              }
          }
      })
      .catch((error) => {
          console.error('Error fetching containers data:', error);
      });
}

// Function to filter notifications by schedule type
function filterBySchedule(notifications, scheduleType, lastDispensed) {
  const currentDate = new Date();
  const lastDispensedDate = new Date(lastDispensed || 0);

  switch (scheduleType) {
      case 'daily':
          return notifications; // No filter needed for daily schedule
      case 'alternative':
          const daysSinceLastDispensed = Math.floor(
              (currentDate - lastDispensedDate) / (1000 * 60 * 60 * 24)
          );
          if (daysSinceLastDispensed >= 2) {
              return notifications; // Dispense only if 2 or more days have passed
          }
          break;
      case 'weekly':
          const weeksSinceLastDispensed = Math.floor(
              (currentDate - lastDispensedDate) / (1000 * 60 * 60 * 24 * 7)
          );
          if (weeksSinceLastDispensed >= 1) {
              return notifications; // Dispense only if 1 or more weeks have passed
          }
          break;
      default:
          console.warn('Unknown schedule type:', scheduleType);
          return []; // Default to no notifications if the schedule type is unknown
  }
  return []; // No notifications match the schedule
}

// Filter upcoming pill times based on the current time
function filterUpcomingTimes(notifications) {
  const currentTime = getCurrentTime();
  const upcoming = notifications.filter((time) => {
      const isUpcoming = compareTime(currentTime, time);
      return isUpcoming;
  });
  return upcoming;
}

// Compare current time with scheduled time (current time should be less than the scheduled time)
function compareTime(currentTime, scheduledTime) {
  const current = convertToMinutes(currentTime);
  const scheduled = convertToMinutes(scheduledTime);
  return current < scheduled;
}

// Convert time in 'hh:mm AM/PM' format to minutes
function convertToMinutes(time) {
  if (time.includes('AM') || time.includes('PM')) {
      const [timeStr, period] = time.split(' ');
      const [hour, minute] = timeStr.split(':').map(Number);

      let totalMinutes = (hour % 12) * 60 + minute;
      if (period === 'PM') {
          totalMinutes += 12 * 60; // Add 12 hours for PM
      }
      return totalMinutes;
  } else {
      const [hour, minute] = time.split(':').map(Number);
      return hour * 60 + minute; // 24-hour format
  }
}

// Get the current time in a readable format
function getCurrentTime() {
  const now = new Date();
  const hours = now.getHours();
  const minutes = now.getMinutes();
  const ampm = hours >= 12 ? 'PM' : 'AM';
  const hourIn12 = hours % 12 || 12;
  return `${hourIn12}:${minutes < 10 ? '0' + minutes : minutes} ${ampm}`;
}

// Run the logic when the page is loaded
window.addEventListener('load', runUpcomingPillLogic);

