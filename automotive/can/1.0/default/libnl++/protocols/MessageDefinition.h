/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <libnl++/Buffer.h>
#include <libnl++/types.h>

#include <map>
#include <sstream>
#include <variant>

namespace android::nl::protocols {

struct AttributeDefinition;

/**
 * Mapping between nlattrtype_t identifier and attribute definition.
 *
 * The map contains values for all nlattrtype_t identifiers - if some is missing, a generic
 * definition with a identifier as its name will be generated.
 *
 * It's possible to define a default attribute to return instead of to_string of its identifier
 * (useful for nested attribute lists). In such case, an entry with id=std::nullopt needs to be
 * present in the map.
 */
class AttributeMap : private std::map<std::optional<nlattrtype_t>, AttributeDefinition> {
  public:
    using std::map<std::optional<nlattrtype_t>, AttributeDefinition>::value_type;

    AttributeMap(const std::initializer_list<value_type> attrTypes);

    const AttributeDefinition operator[](nlattrtype_t nla_type) const;
};

/**
 * Attribute definition.
 *
 * Describes the name and type (optionally sub types, in case of Nested attribute)
 * for a given message attribute.
 */
struct AttributeDefinition {
    enum class DataType : uint8_t {
        Raw,
        Nested,
        String,
        Uint,
        Struct,
    };
    using ToStream = std::function<void(std::stringstream& ss, const Buffer<nlattr> attr)>;

    std::string name;
    DataType dataType = DataType::Raw;
    std::variant<AttributeMap, ToStream> ops = AttributeMap{};
};

/**
 * General message type's kind.
 *
 * For example, RTM_NEWLINK is a NEW kind. For details, please see "Flags values"
 * section in linux/netlink.h.
 */
enum class MessageGenre {
    UNKNOWN,
    GET,
    NEW,
    DELETE,
    ACK,
};

/**
 * Message family descriptor.
 *
 * Describes the structure of all message types with the same header and attributes.
 */
class MessageDescriptor {
  public:
    struct MessageDetails {
        std::string name;
        MessageGenre genre;
    };
    typedef std::map<nlmsgtype_t, MessageDetails> MessageDetailsMap;

  public:
    virtual ~MessageDescriptor();

    size_t getContentsSize() const;
    const MessageDetailsMap& getMessageDetailsMap() const;
    const AttributeMap& getAttributeMap() const;
    MessageDetails getMessageDetails(nlmsgtype_t msgtype) const;
    virtual void dataToStream(std::stringstream& ss, const Buffer<nlmsghdr> hdr) const = 0;

    static MessageDetails getMessageDetails(
            const std::optional<std::reference_wrapper<const MessageDescriptor>>& msgDescMaybe,
            nlmsgtype_t msgtype);

  protected:
    MessageDescriptor(const std::string& name, const MessageDetailsMap&& messageDetails,
                      const AttributeMap&& attrTypes, size_t contentsSize);

  private:
    const std::string mName;
    const size_t mContentsSize;
    const MessageDetailsMap mMessageDetails;
    const AttributeMap mAttributeMap;
};

/**
 * Message definition template.
 *
 * A convenience initialization helper of a message descriptor.
 */
template <typename T>
class MessageDefinition : public MessageDescriptor {
  public:
    MessageDefinition(  //
            const std::string& name,
            const std::initializer_list<MessageDescriptor::MessageDetailsMap::value_type> msgDet,
            const std::initializer_list<AttributeMap::value_type> attrTypes = {})
        : MessageDescriptor(name, msgDet, attrTypes, sizeof(T)) {}

    void dataToStream(std::stringstream& ss, const Buffer<nlmsghdr> hdr) const override {
        const auto& [ok, msg] = hdr.data<T>().getFirst();
        if (!ok) {
            ss << "{incomplete payload}";
            return;
        }

        toStream(ss, msg);
    }

  protected:
    virtual void toStream(std::stringstream& ss, const T& data) const = 0;
};

}  // namespace android::nl::protocols
