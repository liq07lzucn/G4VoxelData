//////////////////////////////////////////////////////////////////////////
// G4VoxelData
// ===========
// A general interface for loading voxelised data as geometry in GEANT4.
//
// Author:  Christopher M Poole <mail@christopherpoole.net>
// Source:  http://github.com/christopherpoole/G4VoxelData
//
// License & Copyright
// ===================
// 
// Copyright 2013 Christopher M Poole <mail@christopherpoole.net>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////

// G4VOXELDATA //
#include "G4VoxelData.hh"
#include "G4VoxelArray.hh"

#ifndef G4VOXELDATAPARAMETERISATION_HH
#define G4VOXELDATAPARAMETERISATION_HH

// STL //
#include <vector>
#include <map>

// GEANT4 //
#include "globals.hh"
#include "G4Types.hh"
#include "G4ThreeVector.hh"
#include "G4PVParameterised.hh"
#include "G4VNestedParameterisation.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VTouchable.hh"
#include "G4ThreeVector.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4VisAttributes.hh"


template <typename T>
class G4VoxelDataParameterisation : public G4VNestedParameterisation
{
public:

    G4VoxelDataParameterisation(G4VoxelArray<T>* array,
        std::map<T, G4Material*> materials_map, G4VPhysicalVolume* mother_physical)
    {
        this->array = array;

        this->materials_map = materials_map;
        this->mother_physical = mother_physical;

        this->voxel_size = array->GetVoxelSize();
        this->volume_shape = array->GetVolumeShape();
    
    };

    virtual ~G4VoxelDataParameterisation(){};

    virtual void Construct(G4ThreeVector position, G4RotationMatrix* rotation) {
        G4NistManager* nist_manager = G4NistManager::Instance();
        G4Material* air = nist_manager->FindOrBuildMaterial("G4_AIR");


        G4Box* voxeldata_solid =
            new G4Box("voxeldata_solid", array->shape[0]*array->spacing[0]/2.,
                                         array->shape[1]*array->spacing[1]/2.,
                                         array->shape[2]*array->spacing[2]/2.);
        voxeldata_logical =
            new G4LogicalVolume(voxeldata_solid, air, "voxeldata_logical", 0, 0, 0);
        new G4PVPlacement(rotation, position,
            "voxeldata_container", voxeldata_logical, mother_physical, 0, false, 0);
        voxeldata_logical->SetVisAttributes(G4VisAttributes::Invisible);

        // Y //
        G4VSolid* y_solid =
            new G4Box("y_solid", array->shape[0]*array->spacing[0]/2.,
                                 array->spacing[1]/2.,
                                 array->shape[2]*array->spacing[2]/2.);
        y_logical = new G4LogicalVolume(y_solid, air, "y_logical");
        new G4PVReplica("y_replica", y_logical, voxeldata_logical,
                        kYAxis, array->shape[1], array->spacing[1]);
        y_logical->SetVisAttributes(G4VisAttributes::Invisible);

        // X //
        G4VSolid* x_solid =
            new G4Box("x_solid", array->spacing[0]/2.,
                                 array->spacing[1]/2.,
                                 array->shape[2]*array->spacing[2]/2.);
        x_logical = new G4LogicalVolume(x_solid, air, "x_logical");
        new G4PVReplica("x_replica", x_logical, y_logical, kXAxis, array->shape[0],
                        array->spacing[0]);
        x_logical->SetVisAttributes(G4VisAttributes::Invisible);

        // VOXEL //
        G4VSolid* voxel_solid =
            new G4Box("voxel_solid", array->spacing[0]/2.,
                                     array->spacing[1]/2.,
                                     array->spacing[2]/2.);
        voxel_logical = new G4LogicalVolume(voxel_solid, air, "voxel_logical");
        voxel_logical->SetVisAttributes(G4VisAttributes::Invisible);
        
        new G4PVParameterised("voxel_data", voxel_logical, x_logical, kUndefined, array->shape[2], this);
    };

    using G4VNestedParameterisation::ComputeMaterial;
    G4Material* ComputeMaterial(G4VPhysicalVolume *physical_volume,
            const G4int copy_number, const G4VTouchable *parent_touchable)
    {
        G4int x = parent_touchable->GetReplicaNumber(0);
        G4int y = parent_touchable->GetReplicaNumber(1);
        G4int z = copy_number;

        if (z < 0) z = 0;

        int index = x + (volume_shape.x() * y) + (volume_shape.x() * volume_shape.y() * z);
        G4Material* VoxelMaterial = GetMaterial(index);
        
        //double gray = (array->GetRoundedValue(index, -1000, 2500, 25) + 2500) / 4000.;
        //physical_volume->GetLogicalVolume()->SetVisAttributes(G4Colour(gray, 0, gray, 1));
        
        physical_volume->GetLogicalVolume()->SetMaterial(VoxelMaterial);

        return VoxelMaterial;
    };


    G4int GetNumberOfMaterials() const
    {
        return array->length;
    };

    G4Material* GetMaterial(G4int i) const
    {
        T hu = array->GetRoundedValue(i, -1000, 2000, 25);
        return materials_map.at(hu);
    };

    unsigned int GetMaterialIndex( unsigned int copyNo) const
    {
        return copyNo;   
    };

    void ComputeTransformation(const G4int copyNo, G4VPhysicalVolume *physVol) const
    {
        G4double x = 0;
        G4double y = 0;
        G4double z = (2*copyNo+1)*voxel_size.x()/2. - voxel_size.z()*volume_shape.z()/2.;

        G4ThreeVector position(x, y, z);
        physVol->SetTranslation(position);
    };

    using G4VNestedParameterisation::ComputeDimensions;
    void ComputeDimensions(G4Box& box, const G4int, const G4VPhysicalVolume *) const
    {
        box.SetXHalfLength(voxel_size.x()/2.);
        box.SetYHalfLength(voxel_size.y()/2.);
        box.SetZHalfLength(voxel_size.z()/2.);
    };

    G4LogicalVolume* GetLogicalVolume() {
        return voxel_logical;
    };

    void SetVisibility(G4bool visible) {
    
    };

  private:
    G4ThreeVector voxel_size;
    G4ThreeVector volume_shape;

    std::vector<G4Material*> fMaterials;//array of pointers to materials
    size_t* fMaterialIndices; // Index in materials corresponding to each voxel

    std::map<T, G4Material*> materials_map;
    G4VoxelData* voxel_data;
    G4VoxelArray<T>* array;
    G4bool with_map;

    G4VPhysicalVolume* mother_physical;

    G4LogicalVolume* voxeldata_logical;
    G4LogicalVolume* voxel_logical;
    G4LogicalVolume* x_logical;
    G4LogicalVolume* y_logical;

    G4bool visibility;
};

#endif // G4VOXELDATAPARAMETERISATION_HH