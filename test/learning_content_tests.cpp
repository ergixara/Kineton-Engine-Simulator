#include <gtest/gtest.h>
#include "../include/learning_content.h"

#include <limits>
#include <set>
#include <string>

TEST(LearningContentTests, EverySelectablePartHasUniqueCompleteLesson) {
    std::set<std::string> names;
    for (int i = 0; i < static_cast<int>(EnginePart::Count); ++i) {
        const EnginePart part = static_cast<EnginePart>(i);
        const PartLesson &lesson = partLesson(part);
        SCOPED_TRACE(i);
        EXPECT_EQ(lesson.part, part);
        for (const char *field : {lesson.name, lesson.category, lesson.description,
                                 lesson.interaction, lesson.changeEffect}) {
            ASSERT_NE(field, nullptr);
            EXPECT_NE(field[0], '\0');
        }
        EXPECT_TRUE(names.insert(lesson.name).second);
        EXPECT_GT(lesson.color, 0u);
        EXPECT_LE(lesson.color, 0xFFFFFFu);
    }
}

TEST(LearningContentTests, InvalidSelectionReturnsStableGuideInsteadOfIndexingOutsideTable) {
    const PartLesson &guide = partLesson(EnginePart::None);
    for (int value : {-1, static_cast<int>(EnginePart::Count),
                     std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        EXPECT_EQ(&partLesson(static_cast<EnginePart>(value)), &guide);
    }
}
