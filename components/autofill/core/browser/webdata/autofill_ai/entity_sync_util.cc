// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/autofill_ai_chrome_metadata.pb.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"

namespace autofill {

namespace {

// Wraps a message `m` into an `Any`-typed message, essentially dropping the
// actual type for serialization purposes.
sync_pb::Any AnyWrapProto(const google::protobuf::MessageLite& m) {
  sync_pb::Any any;
  any.set_type_url(base::StrCat({"type.googleapis.com/", m.GetTypeName()}));
  m.SerializeToString(any.mutable_value());
  return any;
}

// Serializes metadata related to `EntityInstance` into
// `ChromeValuablesMetadata`.
sync_pb::Any SerializeChromeValuablesMetadata(const EntityInstance& entity) {
  ChromeValuablesMetadata metadata;
  for (const AttributeInstance& attribute : entity.attributes()) {
    switch (attribute.type().data_type()) {
      case AttributeType::DataType::kName: {
        for (FieldType field_type : attribute.type().field_subtypes()) {
          ChromeValuablesMetadataEntry& entry =
              *metadata.add_metadata_entries();
          entry.set_attribute_type(attribute.type().name_as_string());
          entry.set_field_type(field_type);
          entry.set_value(base::UTF16ToUTF8(attribute.GetRawInfo(field_type)));
          entry.set_verification_status(
              static_cast<int>(attribute.GetVerificationStatus(field_type)));
        }
        break;
      }
      case AttributeType::DataType::kCountry:
      case AttributeType::DataType::kDate:
        // TODO(crbug.com/436174974): Implement serialization of those
        // DataType's.
        NOTIMPLEMENTED();
        break;
      case AttributeType::DataType::kState:
      case AttributeType::DataType::kString:
        // Nothing to serialize here as the structure is trivial.
        break;
    }
  }
  return AnyWrapProto(metadata);
}

// Takes the `serialized_metadata` and populates `attributes` with the
// information that was serialized in it. `entity_type` indicates what type of
// entity does the metadata store information for.
void ReadChromeValuablesMetadata(
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>&
        attributes,
    EntityType entity_type,
    const sync_pb::Any& serialized_metadata) {
  ChromeValuablesMetadata metadata;
  if (!metadata.ParseFromString(serialized_metadata.value())) {
    return;
  }
  for (const ChromeValuablesMetadataEntry& entry :
       metadata.metadata_entries()) {
    std::optional<AttributeType> attribute_type =
        StringToAttributeType(entity_type, entry.attribute_type());
    FieldType field_type = ToSafeFieldType(entry.field_type(), UNKNOWN_TYPE);
    std::u16string value = base::UTF8ToUTF16(entry.value());
    std::optional<VerificationStatus> status =
        ToSafeVerificationStatus(entry.verification_status());
    if (!attribute_type || field_type == UNKNOWN_TYPE || !status) {
      continue;
    }
    auto it = attributes.find(*attribute_type);
    AttributeInstance& attribute =
        it != attributes.end()
            ? *it
            : *attributes.insert(AttributeInstance(*attribute_type)).first;
    attribute.SetRawInfo(field_type, value, *status);
  }
}

// Reads the `specifics` message and extract attribute-information from its
// different fields. In particular, it also deserializes the metadata stored in
// the sync message.
base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
GetFlightReservationAttributesFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  using enum AttributeTypeName;
  CHECK_EQ(specifics.valuable_data_case(),
           sync_pb::AutofillValuableSpecifics::kFlightReservation);
  const sync_pb::FlightReservation& flight_reservation =
      specifics.flight_reservation();
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;
  auto add_attribute = [&](AttributeTypeName attribute_type_name,
                           std::string value) {
    AttributeInstance attribute((AttributeType(attribute_type_name)));
    // Setting the VerificationStatus to `kNoStatus` is fine because it is only
    // relevant for name types, and for those the status will later be set
    // correctly in `ReadChromeValuablesMetadata`.
    attribute.SetRawInfo(attribute.type().field_type(),
                         base::UTF8ToUTF16(value),
                         VerificationStatus::kNoStatus);
    attributes.insert(std::move(attribute));
  };

  add_attribute(kFlightReservationFlightNumber,
                flight_reservation.flight_number());
  add_attribute(kFlightReservationTicketNumber,
                flight_reservation.flight_ticket_number());
  add_attribute(kFlightReservationConfirmationCode,
                flight_reservation.flight_confirmation_code());
  add_attribute(kFlightReservationPassengerName,
                flight_reservation.passenger_name());
  add_attribute(kFlightReservationDepartureAirport,
                flight_reservation.departure_airport());
  add_attribute(kFlightReservationArrivalAirport,
                flight_reservation.arrival_airport());

  ReadChromeValuablesMetadata(attributes,
                              EntityType(EntityTypeName::kFlightReservation),
                              specifics.serialized_chrome_valuables_metadata());
  for (AttributeInstance& attribute : attributes) {
    attribute.FinalizeInfo();
  }
  return attributes;
}

// Takes an `entity` and returns a proto message with the information needed
// in order to send this entity to the sync server.
sync_pb::AutofillValuableSpecifics GetFlightReservationSpecifics(
    const EntityInstance& entity) {
  using enum AttributeTypeName;
  CHECK_EQ(entity.type().name(), EntityTypeName::kFlightReservation);
  auto get_value = [&](AttributeTypeName attribute_type_name) {
    return base::UTF16ToUTF8(
        entity.attribute(AttributeType(attribute_type_name))
            ->GetCompleteRawInfo());
  };
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(*entity.guid());
  specifics.set_is_editable(!entity.are_attributes_read_only());

  sync_pb::FlightReservation& flight_reservation =
      *specifics.mutable_flight_reservation();
  flight_reservation.set_flight_number(
      get_value(kFlightReservationFlightNumber));
  flight_reservation.set_flight_ticket_number(
      get_value(kFlightReservationTicketNumber));
  flight_reservation.set_flight_confirmation_code(
      get_value(kFlightReservationConfirmationCode));
  flight_reservation.set_passenger_name(
      get_value(kFlightReservationPassengerName));
  flight_reservation.set_departure_airport(
      get_value(kFlightReservationDepartureAirport));
  flight_reservation.set_arrival_airport(
      get_value(kFlightReservationArrivalAirport));

  *specifics.mutable_serialized_chrome_valuables_metadata() =
      SerializeChromeValuablesMetadata(entity);
  return specifics;
}

// Takes an `entity` and returns a proto message with the information needed in
// order to send this entity to the sync server.
sync_pb::AutofillValuableSpecifics GetVehicleInformationSpecifics(
    const EntityInstance& entity) {
  using enum AttributeTypeName;
  CHECK_EQ(entity.type().name(), EntityTypeName::kVehicle);
  sync_pb::AutofillValuableSpecifics specifics;
  specifics.set_id(*entity.guid());
  specifics.set_is_editable(!entity.are_attributes_read_only());

  sync_pb::VehicleRegistration& vehicle =
      *specifics.mutable_vehicle_registration();
  auto set_vehicle_field =
      [&](AttributeTypeName attribute_type_name,
          void (sync_pb::VehicleRegistration::*setter)(const std::string&)) {
        if (base::optional_ref<const AttributeInstance> attribute =
                entity.attribute(AttributeType(attribute_type_name))) {
          (vehicle.*setter)(base::UTF16ToUTF8(attribute->GetCompleteRawInfo()));
        }
      };

  set_vehicle_field(kVehicleMake,
                    &sync_pb::VehicleRegistration::set_vehicle_make);
  set_vehicle_field(kVehicleModel,
                    &sync_pb::VehicleRegistration::set_vehicle_model);
  set_vehicle_field(kVehicleYear,
                    &sync_pb::VehicleRegistration::set_vehicle_year);
  set_vehicle_field(
      kVehicleVin,
      &sync_pb::VehicleRegistration::set_vehicle_identification_number);
  set_vehicle_field(kVehiclePlateNumber,
                    &sync_pb::VehicleRegistration::set_vehicle_license_plate);
  set_vehicle_field(kVehiclePlateState,
                    &sync_pb::VehicleRegistration::set_license_plate_region);
  set_vehicle_field(kVehicleOwner,
                    &sync_pb::VehicleRegistration::set_owner_name);

  *specifics.mutable_serialized_chrome_valuables_metadata() =
      SerializeChromeValuablesMetadata(entity);
  return specifics;
}

// Reads the `specifics` message and extract attribute-information from its
// different fields. In particular, it also deserializes the metadata stored in
// the sync message.
base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
GetVehicleAttributesFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  using enum AttributeTypeName;
  CHECK_EQ(specifics.valuable_data_case(),
           sync_pb::AutofillValuableSpecifics::kVehicleRegistration);
  const sync_pb::VehicleRegistration& vehicle =
      specifics.vehicle_registration();
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;
  auto add_attribute = [&](AttributeTypeName attribute_type_name,
                           std::string value) {
    AttributeInstance attribute((AttributeType(attribute_type_name)));
    // Setting the VerificationStatus to `kNoStatus` is fine because it is only
    // relevant for name types, and for those the status will later be set
    // correctly in `ReadChromeValuablesMetadata`.
    attribute.SetRawInfo(attribute.type().field_type(),
                         base::UTF8ToUTF16(value),
                         VerificationStatus::kNoStatus);
    attributes.insert(std::move(attribute));
  };

  add_attribute(kVehicleMake, vehicle.vehicle_make());
  add_attribute(kVehicleModel, vehicle.vehicle_model());
  add_attribute(kVehicleYear, vehicle.vehicle_year());
  add_attribute(kVehicleVin, vehicle.vehicle_identification_number());
  add_attribute(kVehiclePlateNumber, vehicle.vehicle_license_plate());
  add_attribute(kVehiclePlateState, vehicle.license_plate_region());
  add_attribute(kVehicleOwner, vehicle.owner_name());

  ReadChromeValuablesMetadata(attributes, EntityType(EntityTypeName::kVehicle),
                              specifics.serialized_chrome_valuables_metadata());
  for (AttributeInstance& attribute : attributes) {
    attribute.FinalizeInfo();
  }
  return attributes;
}

}  // namespace

sync_pb::AutofillValuableSpecifics CreateSpecificsFromEntityInstance(
    const EntityInstance& entity) {
  switch (entity.type().name()) {
    case EntityTypeName::kFlightReservation:
      return GetFlightReservationSpecifics(entity);
    case EntityTypeName::kVehicle:
      return GetVehicleInformationSpecifics(entity);
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      // Those entity types are not synced.
      NOTREACHED();
  }
  NOTREACHED();
}

std::optional<EntityInstance> CreateEntityInstanceFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  const EntityInstance::EntityId guid(specifics.id());
  switch (specifics.valuable_data_case()) {
    case sync_pb::AutofillValuableSpecifics::kVehicleRegistration: {
      return EntityInstance(
          EntityType(EntityTypeName::kVehicle),
          GetVehicleAttributesFromSpecifics(specifics), guid,
          /*nickname=*/"", /*date_modified=*/{}, /*use_count=*/{},
          /*use_date=*/{}, EntityInstance::RecordType::kServerWallet,
          EntityInstance::AreAttributesReadOnly(!specifics.is_editable()),
          /*frecency_override=*/"");
    }
    case sync_pb::AutofillValuableSpecifics::kFlightReservation: {
      std::string frecency_override;
      if (specifics.flight_reservation()
              .has_departure_date_unix_epoch_micros()) {
        base::Time departure_time = base::Time::FromMillisecondsSinceUnixEpoch(
            specifics.flight_reservation().departure_date_unix_epoch_micros() /
            1000);
        frecency_override = base::TimeFormatAsIso8601(departure_time);
      }
      return EntityInstance(
          EntityType(EntityTypeName::kFlightReservation),
          GetFlightReservationAttributesFromSpecifics(specifics), guid,
          /*nickname=*/"", /*date_modified=*/{}, /*use_count=*/{},
          /*use_date=*/{}, EntityInstance::RecordType::kServerWallet,
          EntityInstance::AreAttributesReadOnly(!specifics.is_editable()),
          frecency_override);
    }
    case sync_pb::AutofillValuableSpecifics::kLoyaltyCard:
    case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
      // Such specifics shouldn't reach this function as they aren't supported
      // by AutofillAi.
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace autofill
