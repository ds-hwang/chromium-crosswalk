// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_database_data.pb.h"
#include "content/browser/notifications/notification_database_data_conversions.h"
#include "content/common/notification_constants.h"
#include "content/public/browser/notification_database_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const int64_t kNotificationId = 42;
const int64_t kServiceWorkerRegistrationId = 9001;

const char kOrigin[] = "https://example.com/";
const char kNotificationTitle[] = "My Notification";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world!";
const char kNotificationTag[] = "my_tag";
const char kNotificationIconUrl[] = "https://example.com/icon.png";
const char kNotificationActionIconUrl[] = "https://example.com/action_icon.png";
const int kNotificationVibrationPattern[] = {100, 200, 300};
const double kNotificationTimestamp = 621046800.;
const unsigned char kNotificationData[] = {0xdf, 0xff, 0x0, 0x0, 0xff, 0xdf};

TEST(NotificationDatabaseDataTest, SerializeAndDeserializeData) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern + arraysize(kNotificationVibrationPattern));

  std::vector<char> developer_data(
      kNotificationData, kNotificationData + arraysize(kNotificationData));

  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16(kNotificationTitle);
  notification_data.direction =
      PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
  notification_data.lang = kNotificationLang;
  notification_data.body = base::ASCIIToUTF16(kNotificationBody);
  notification_data.tag = kNotificationTag;
  notification_data.icon = GURL(kNotificationIconUrl);
  notification_data.vibration_pattern = vibration_pattern;
  notification_data.timestamp = base::Time::FromJsTime(kNotificationTimestamp);
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;
  notification_data.data = developer_data;
  for (size_t i = 0; i < kPlatformNotificationMaxActions; i++) {
    PlatformNotificationAction notification_action;
    notification_action.action = base::SizeTToString(i);
    notification_action.title = base::SizeTToString16(i);
    notification_action.icon = GURL(kNotificationActionIconUrl);
    notification_data.actions.push_back(notification_action);
  }

  NotificationDatabaseData database_data;
  database_data.notification_id = kNotificationId;
  database_data.origin = GURL(kOrigin);
  database_data.service_worker_registration_id = kServiceWorkerRegistrationId;
  database_data.notification_data = notification_data;

  std::string serialized_data;

  // Serialize the data in |notification_data| to the string |serialized_data|.
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(database_data, &serialized_data));

  NotificationDatabaseData copied_data;

  // Deserialize the data in |serialized_data| to |copied_data|.
  ASSERT_TRUE(
      DeserializeNotificationDatabaseData(serialized_data, &copied_data));

  EXPECT_EQ(database_data.notification_id, copied_data.notification_id);
  EXPECT_EQ(database_data.origin, copied_data.origin);
  EXPECT_EQ(database_data.service_worker_registration_id,
            copied_data.service_worker_registration_id);

  const PlatformNotificationData& copied_notification_data =
      copied_data.notification_data;

  EXPECT_EQ(notification_data.title, copied_notification_data.title);
  EXPECT_EQ(notification_data.direction, copied_notification_data.direction);
  EXPECT_EQ(notification_data.lang, copied_notification_data.lang);
  EXPECT_EQ(notification_data.body, copied_notification_data.body);
  EXPECT_EQ(notification_data.tag, copied_notification_data.tag);
  EXPECT_EQ(notification_data.icon, copied_notification_data.icon);

  EXPECT_THAT(copied_notification_data.vibration_pattern,
              testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_EQ(notification_data.timestamp, copied_notification_data.timestamp);
  EXPECT_EQ(notification_data.renotify, copied_notification_data.renotify);
  EXPECT_EQ(notification_data.silent, copied_notification_data.silent);
  EXPECT_EQ(notification_data.require_interaction,
            copied_notification_data.require_interaction);

  ASSERT_EQ(developer_data.size(), copied_notification_data.data.size());
  for (size_t i = 0; i < developer_data.size(); ++i)
    EXPECT_EQ(developer_data[i], copied_notification_data.data[i]);

  ASSERT_EQ(notification_data.actions.size(),
            copied_notification_data.actions.size());
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    EXPECT_EQ(notification_data.actions[i].action,
              copied_notification_data.actions[i].action);
    EXPECT_EQ(notification_data.actions[i].title,
              copied_notification_data.actions[i].title);
    EXPECT_EQ(notification_data.actions[i].icon,
              copied_notification_data.actions[i].icon);
  }
}

TEST(NotificationDatabaseDataTest, SerializeAndDeserializeDirections) {
  PlatformNotificationData::Direction directions[] = {
      PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT,
      PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT,
      PlatformNotificationData::DIRECTION_AUTO};

  for (size_t i = 0; i < arraysize(directions); ++i) {
    PlatformNotificationData notification_data;
    notification_data.direction = directions[i];

    NotificationDatabaseData database_data;
    database_data.notification_data = notification_data;

    std::string serialized_data;
    ASSERT_TRUE(
        SerializeNotificationDatabaseData(database_data, &serialized_data));

    NotificationDatabaseData copied_data;
    ASSERT_TRUE(
        DeserializeNotificationDatabaseData(serialized_data, &copied_data));

    EXPECT_EQ(directions[i], copied_data.notification_data.direction);
  }
}

}  // namespace content
